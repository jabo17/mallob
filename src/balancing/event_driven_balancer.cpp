
#include <climits>

#include "event_driven_balancer.hpp"
#include "util/random.hpp"
#include "app/job.hpp"
#include "volume_calculator.hpp"

EventDrivenBalancer::EventDrivenBalancer(MPI_Comm& comm, Parameters& params) : _comm(comm), _params(params) {

    int size = MyMpi::size(_comm);
    int myRank = MyMpi::rank(_comm);

    // Root rank
    _root_rank = 0;

    if (size == 1) return;

    // Parent rank
    {
        int exp = 2;
        if (myRank == 0) _parent_rank = 0;
        else while (true) {
            if (myRank % exp == exp/2 
                    && myRank - exp/2 >= 0) {
                _parent_rank = myRank - exp/2;
                break;
            }
            exp *= 2;
        }
    }

    // Child ranks
    {
        int exp = 1; while (exp < size) exp *= 2;
        while (true) {
            if (myRank % exp == 0) {
                int child = myRank + exp/2;
                if (child < size) {
                    _child_ranks.push_back(child);
                } 
            }
            exp /= 2;
            if (exp == 1) break;
        }
    }

    log(V5_DEBG, "BLC_TREE parent: %i\n", getParentRank());
    log(V5_DEBG, "BLC_TREE children: ");
    for (int child : getChildRanks()) log(LOG_NO_PREFIX | V5_DEBG, "%i ", child);
    log(LOG_NO_PREFIX | V5_DEBG, ".\n");
}

void EventDrivenBalancer::setVolumeUpdateCallback(std::function<void(int, int, float)> callback) {
    _volume_update_callback = callback;
}

void EventDrivenBalancer::onActivate(const Job& job, int demand) {
    
    if (_active_job_id == job.getId()) {
        onDemandChange(job, demand);
        return;
    }
    _active_job_id = job.getId();
    
    if (!job.getJobTree().isRoot()) return;
    
    if (!_job_root_epochs.count(job.getId())) _job_root_epochs[job.getId()] = 0;
    pushEvent(Event({
        job.getId(), ++_job_root_epochs[job.getId()], std::max(1, demand), job.getPriority()
    }));
}

void EventDrivenBalancer::onDemandChange(const Job& job, int demand) {

    assert(_active_job_id == job.getId());
    assert(job.getJobTree().isRoot());
    assert(_job_root_epochs.at(job.getId()) > 0);

    pushEvent(Event({
        job.getId(), ++_job_root_epochs[job.getId()], demand, job.getPriority()
    }));
}

void EventDrivenBalancer::onSuspend(const Job& job) {

    if (_active_job_id == job.getId()) _active_job_id = -1;
    if (!job.getJobTree().isRoot()) return;

    assert(_job_root_epochs.at(job.getId()) > 0);
    pushEvent(Event({
        job.getId(), ++_job_root_epochs[job.getId()], /*demand=*/0, job.getPriority()
    }));
}

void EventDrivenBalancer::onTerminate(const Job& job) {

    if (_active_job_id == job.getId()) {
        _active_job_id = -1;
        _pending_entries.clear();
    } 
    if (!job.getJobTree().isRoot()) return;

    assert(_job_root_epochs.at(job.getId()) > 0);
    pushEvent(Event({
        job.getId(), /*jobEpoch=*/INT_MAX, /*demand=*/0, /*priority=*/0 
    }));
    _job_root_epochs.erase(job.getId());

    if (!_balancing_latencies.count(job.getId())) return;
    auto& latencies = _balancing_latencies[job.getId()];
    if (!latencies.empty()) {
        std::sort(latencies.begin(), latencies.end());
        float avgLatency = std::accumulate(latencies.begin(), latencies.end(), 0.0f) / latencies.size();
        log(V3_VERB, "%s balancing latency={num:%i min:%.5f med:%.5f avg:%.5f max:%.5f}\n", 
            job.toStr(), latencies.size(), latencies.front(), latencies[latencies.size()/2], avgLatency, latencies.back());
    }
    _balancing_latencies.erase(job.getId());
}

void EventDrivenBalancer::pushEvent(const Event& event) {
    bool inserted = _diffs.insertIfNovel(event);
    if (inserted) {
        _pending_entries.insert(PendingEvent{event.jobId, event.demand, event.priority, Timer::elapsedSeconds()});
        log(V1_WARN, "[WARN] insert (%i,%i,%.3f)\n", event.jobId, event.demand, event.priority);
        advance();
    }
}

void EventDrivenBalancer::advance() {
    // Have anything to reduce?
    if (_diffs.isEmpty()) return;

    // Enough time passed since last balancing?
    if (!_periodic_balancing.ready()) return;

    log(V4_VVER, "Initiate balancing (%i diffs)\n", _diffs.getEntries().size());

    handleData(_diffs, MSG_REDUCE_DATA);
}

void EventDrivenBalancer::handle(MessageHandle& handle) {
    auto data = Serializable::get<EventMap>(handle.getRecvData());
    handleData(data, handle.tag);
}

void EventDrivenBalancer::handleData(EventMap& data, int tag) {
    if (tag == MSG_REDUCE_DATA) {
        if (isRoot(MyMpi::rank(MPI_COMM_WORLD))) {
            // Switch to broadcast, continue below @ other branch
            data.bumpGlobalEpoch();
            tag = MSG_BROADCAST_DATA;
        } else {
            // Reduce further upwards
            MyMpi::isend(getParentRank(), MSG_REDUCE_DATA, data);
        }
    }
    if (tag == MSG_BROADCAST_DATA) {
        // Inner node: Broadcast further downwards
        if (!isLeaf(MyMpi::rank(MPI_COMM_WORLD))) {
            for (auto child : getChildRanks()) { // TODO inefficient calculation of children
                MyMpi::isend(child, MSG_BROADCAST_DATA, data);
            }
        }
        // Digest locally
        digest(data);
    }
}

void EventDrivenBalancer::digest(const EventMap& data) {
    
    log(V4_VVER, "BLC DIGEST epoch=%ld size=%ld\n", data.getGlobalEpoch(), data.getEntries().size());
    //log(V4_VVER, "BLC DIGEST data=%s\n", data.toStr().c_str());
    //log(V4_VVER, "BLC DIGEST states=%s\n", _states.toStr().c_str());
    //log(V4_VVER, "BLC DIGEST diff=%s\n", _diffs.toStr().c_str());

    _states.updateBy(data);
    _balancing_epoch = data.getGlobalEpoch();

    computeBalancingResult();

    // Filter local diffs by the new "global" state.
    size_t diffSize = _diffs.getEntries().size();
    _diffs.filterBy(_states);

    log(V4_VVER, "BLC digest %i diffs, %i/%i local diffs remaining\n", 
            data.getEntries().size(), _diffs.getEntries().size(), diffSize);
    _states.removeOldZeros();
}

void EventDrivenBalancer::computeBalancingResult() {

    float now = Timer::elapsedSeconds();
    int rank = MyMpi::rank(MPI_COMM_WORLD);
    int verb = rank == 0 ? V4_VVER : V5_DEBG;
    _job_volumes.clear();

    if (_states.isEmpty()) return;

    log(V5_DEBG, "BLC: calc result\n");

    VolumeCalculator calc(_states, _params, MyMpi::size(_comm), verb);
    calc.calculateResult();

    std::string msg = "";
    for (const auto& entry : calc.getEntries()) {
        msg += std::to_string(entry.jobId) + ":" + std::to_string(entry.volume) + " ";
        _job_volumes[entry.jobId] = entry.volume;
        if (entry.jobId == _active_job_id) {
            float elapsed = 0;
            PendingEvent ev{_active_job_id, entry.originalDemand, entry.priority, 0};
            if (_pending_entries.count(ev)) {

                auto it = _pending_entries.find(ev);
                elapsed = Timer::elapsedSeconds() - it->time;
                _balancing_latencies[entry.jobId].push_back(elapsed);
                _pending_entries.erase(it);
            }
            
            _volume_update_callback(entry.jobId, entry.volume, elapsed);
        }
    }
    log(rank == 0 ? V3_VERB : V4_VVER, "BLC %s\n", msg.c_str());
}

bool EventDrivenBalancer::hasVolume(int jobId) const {
    return _job_volumes.count(jobId);
}

int EventDrivenBalancer::getVolume(int jobId) const {
    return _job_volumes.at(jobId);
}

int EventDrivenBalancer::getRootRank() {
    return _root_rank;
}
int EventDrivenBalancer::getParentRank() {
    return _parent_rank;
}
const std::vector<int>& EventDrivenBalancer::getChildRanks() {
    return _child_ranks;
}
bool EventDrivenBalancer::isRoot(int rank) {
    return rank == getRootRank();
}
bool EventDrivenBalancer::isLeaf(int rank) {
    return rank % 2 == 1;
}

int EventDrivenBalancer::getNewDemand(int jobId) {
    return _states.getEntries().at(jobId).demand;
}

float EventDrivenBalancer::getPriority(int jobId) {
    return _states.getEntries().at(jobId).priority;
}

size_t EventDrivenBalancer::getGlobalEpoch() const {
    return _states.getGlobalEpoch();
}
