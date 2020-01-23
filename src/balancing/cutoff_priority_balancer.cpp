
#include <utility>
#include <assert.h>

#include "cutoff_priority_balancer.h"
#include "util/random.h"
#include "util/console.h"


bool CutoffPriorityBalancer::beginBalancing(std::map<int, Job*>& jobs) {

    // Initialize
    _assignments.clear();
    _priorities.clear();
    _demands.clear();
    _resources_info = ResourcesInfo();
    _stage = INITIAL_DEMAND;
    _balancing = true;

    // Identify jobs to balance
    bool isWorkerBusy = false;
    _jobs_being_balanced = std::map<int, Job*>();
    assert(_local_jobs == NULL || Console::fail("Found localJobs instance of size %i", _local_jobs->size()));
    _local_jobs = new std::set<int, PriorityComparator>(PriorityComparator(jobs));
    for (auto it : jobs) {
        // Node must be root node to participate
        bool participates = it.second->isRoot();
        // Job must be active, or must be initializing and already having the description
        participates &= it.second->isInState({JobState::ACTIVE, JobState::STANDBY})
                        || (it.second->isInState({JobState::INITIALIZING_TO_ACTIVE}) 
                            && it.second->hasJobDescription());
        if (participates) {
            _jobs_being_balanced[it.first] = it.second;
            _local_jobs->insert(it.first);
        }
        // Set "busy" flag of this worker node to true, if applicable
        if (it.second->isInState({JobState::ACTIVE, JobState::INITIALIZING_TO_ACTIVE})) {
            isWorkerBusy = true;
        }
    }

    // Find global aggregation of demands and amount of busy nodes
    float aggregatedDemand = 0;
    for (auto it : *_local_jobs) {
        int jobId = it;
        _demands[jobId] = getDemand(*_jobs_being_balanced[jobId]);
        _priorities[jobId] = _jobs_being_balanced[jobId]->getDescription().getPriority();
        aggregatedDemand += _demands[jobId] * _priorities[jobId];
        Console::log(Console::VERB, "Job #%i : demand %i", jobId, _demands[jobId]);
    }
    Console::log(Console::VERB, "Local aggregated demand: %.3f", aggregatedDemand);
    _demand_and_busy_nodes_contrib[0] = aggregatedDemand;
    _demand_and_busy_nodes_contrib[1] = (isWorkerBusy ? 1 : 0);
    _demand_and_busy_nodes_result[0] = 0;
    _demand_and_busy_nodes_result[1] = 0;
    _reduce_request = MyMpi::iallreduce(_comm, _demand_and_busy_nodes_contrib, _demand_and_busy_nodes_result, 2);

    return false; // not finished yet: wait for end of iallreduce
}

bool CutoffPriorityBalancer::canContinueBalancing() {
    if (_stage == INITIAL_DEMAND) {
        // Check if reduction is done
        int flag = 0;
        MPI_Status status;
        MPI_Test(&_reduce_request, &flag, &status);
        return flag;
    }
    if (_stage == REDUCE_RESOURCES || _stage == BROADCAST_RESOURCES) {
        return false; // balancing is advanced by an individual message
    }
    if (_stage == REDUCE_REMAINDERS || _stage == BROADCAST_REMAINDERS) {
        return false; // balancing is advanced by an individual message
    }
    if (_stage == GLOBAL_ROUNDING) {
        // Check if reduction is done
        int flag = 0;
        MPI_Status status;
        MPI_Test(&_reduce_request, &flag, &status);
        return flag;
    }
    return false;
}

bool CutoffPriorityBalancer::continueBalancing() {
    if (_stage == INITIAL_DEMAND) {

        // Finish up initial reduction
        float aggregatedDemand = _demand_and_busy_nodes_result[0];
        int busyNodes = _demand_and_busy_nodes_result[1];
        bool rankZero = MyMpi::rank(MPI_COMM_WORLD) == 0;
        Console::log(rankZero ? Console::VVERB : Console::VVVVERB, 
            "%i/%i nodes (%.2f%%) are busy", (int)busyNodes, MyMpi::size(_comm), 
            ((float)100*busyNodes)/MyMpi::size(_comm));
        Console::log(rankZero ? Console::VVERB : Console::VVVVERB, 
            "Aggregation of demands: %.3f", aggregatedDemand);

        // Calculate local initial assignments
        _total_volume = (int) (MyMpi::size(_comm) * _load_factor);
        for (auto it : *_local_jobs) {
            int jobId = it;
            float initialMetRatio = _total_volume * _priorities[jobId] / aggregatedDemand;
            _assignments[jobId] = std::min(1.0f, initialMetRatio) * _demands[jobId];
            Console::log(Console::VVERB, "Job #%i : initial assignment %.3f", jobId, _assignments[jobId]);
        }

        // Create ResourceInfo instance with local data
        for (auto it : *_local_jobs) {
            int jobId = it;
            _resources_info.assignedResources += _assignments[jobId];
            _resources_info.priorities.push_back(_priorities[jobId]);
            _resources_info.demandedResources.push_back( _demands[jobId] - _assignments[jobId] );
        }

        // Continue
        return continueBalancing(NULL);
    }

    if (_stage == GLOBAL_ROUNDING) {
        return continueRoundingFromReduction();
    }
    return false;
}

bool CutoffPriorityBalancer::continueBalancing(MessageHandlePtr handle) {
    bool done;
    BalancingStage stage = _stage;
    if (_stage == INITIAL_DEMAND) {
        _stage = REDUCE_RESOURCES;
    }
    if (_stage == REDUCE_RESOURCES) {
        if (handle == NULL || stage != REDUCE_RESOURCES) {
            done = _resources_info.startReduction(_comm);
        } else {
            done = _resources_info.advanceReduction(handle);
        }
        if (done) _stage = BROADCAST_RESOURCES;
    }
    if (_stage == BROADCAST_RESOURCES) {
        if (handle == NULL || stage != BROADCAST_RESOURCES) {
            done = _resources_info.startBroadcast(_comm, _resources_info.getExcludedRanks());
        } else {
            done = _resources_info.advanceBroadcast(handle);
        }
        if (done) {
            return finishResourcesReduction();
        }
    }

    if (_stage == REDUCE_REMAINDERS) {
        if (handle == NULL || stage != REDUCE_REMAINDERS) {
            done = _remainders.startReduction(_comm, _resources_info.getExcludedRanks());
        } else {
            done = _remainders.advanceReduction(handle);
        }
        if (done) _stage = BROADCAST_REMAINDERS;
    }
    if (_stage == BROADCAST_REMAINDERS) {
        if (handle == NULL || stage != BROADCAST_REMAINDERS) {
            done = _remainders.startBroadcast(_comm, _remainders.getExcludedRanks());
        } else {
            done = _remainders.advanceBroadcast(handle);
        }
        if (done) {
            done = finishRemaindersReduction();
            _stage = GLOBAL_ROUNDING;
            return done;
        }
    }
    return false;
}

bool CutoffPriorityBalancer::finishResourcesReduction() {

    _stats.increment("reductions"); _stats.increment("broadcasts");

    // "resourcesInfo" now contains global data from all concerned jobs
    if (_resources_info.getExcludedRanks().count(MyMpi::rank(_comm)) && !ITERATIVE_ROUNDING) {
        Console::log(Console::VVERB, "Ended all-reduction. Balancing finished.");
        _balancing = false;
        delete _local_jobs;
        _local_jobs = NULL;
        _assignments = std::map<int, float>();
        return true;
    } else {
        Console::log(Console::VVERB, "Ended all-reduction. Calculating final job demands");
    }

    // Assign correct (final) floating-point resources
    int rank = MyMpi::rank(MPI_COMM_WORLD);
    Console::log(!rank ? Console::VVERB : Console::VVVVERB, "Initially assigned resources: %.3f", 
        _resources_info.assignedResources);
    
    float remainingResources = _total_volume - _resources_info.assignedResources;
    if (remainingResources < 0.1) remainingResources = 0;

    Console::log(!rank ? Console::VVERB : Console::VVVVERB, "Remaining resources: %.3f", remainingResources);

    for (auto it : _jobs_being_balanced) {
        int jobId = it.first;

        float demand = _demands[jobId];
        float priority = _priorities[jobId];
        std::vector<float>& priorities = _resources_info.priorities;
        std::vector<float>& demandedResources = _resources_info.demandedResources;
        std::vector<float>::iterator itPrio = std::find(priorities.begin(), priorities.end(), priority);
        assert(itPrio != priorities.end() || Console::fail("Priority %.3f not found in histogram!", priority));
        int prioIndex = std::distance(priorities.begin(), itPrio);

        if (_assignments[jobId] == demand
            || priorities[prioIndex] <= remainingResources) {
            // Case 1: Assign full demand
            _assignments[jobId] = demand;
        } else {
            if (prioIndex == 0 || demandedResources[prioIndex-1] >= remainingResources) {
                // Case 2: No additional resources assigned
            } else {
                // Case 3: Evenly distribute ratio of remaining resources
                assert(remainingResources >= 0);
                float ratio = (remainingResources - demandedResources[prioIndex-1])
                            / (demandedResources[prioIndex] - demandedResources[prioIndex-1]);
                assert(ratio > 0);
                assert(ratio <= 1);
                _assignments[jobId] += ratio * (demand - _assignments[jobId]);
            }
        }
    }

    if (ITERATIVE_ROUNDING)  {
        // Build contribution to all-reduction of non-zero remainders
        _remainders = SortedDoubleSequence();
        for (auto it : _jobs_being_balanced) {
            int jobId = it.first;
            if (_assignments[jobId] < 1) continue;
            double remainder = _assignments[jobId] - (int)_assignments[jobId];
            if (remainder > 0 && remainder < 1) _remainders.add(remainder);
        }
        _last_utilization = 0;
        _best_remainder_idx = -1;

        _stage = REDUCE_REMAINDERS;
        return continueBalancing(NULL);

    } else return true;    
}

bool CutoffPriorityBalancer::finishRemaindersReduction() {
    if (!_remainders.isEmpty()) {
        Console::getLock();
        Console::appendUnsafe(Console::VVVERB, "Seq. of remainders: ");
        for (int i = 0; i < _remainders.size(); i++) Console::appendUnsafe(Console::VVVERB, "%.3f ", _remainders[i]);
        Console::logUnsafe(Console::VVVERB, "");
        Console::releaseLock();
    }
    return continueRoundingUntilReduction(0, _remainders.size());
}

std::map<int, int> getRoundedAssignments(std::map<int, float>& assignments, double remainder, int& sum) {
    std::map<int, int> roundedAssignments;
    for (auto it : assignments) {
        if (it.second <= 1) {
            // special treatment for atomic jobs
            roundedAssignments[it.first] = 1;
            sum++;
            continue;
        } 
        double r = it.second - (int)it.second;
        if (r < remainder) roundedAssignments[it.first] = std::floor(it.second);
        if (r >= remainder) roundedAssignments[it.first] = std::ceil(it.second);
        sum += roundedAssignments[it.first];
    }
    return roundedAssignments;
}

bool CutoffPriorityBalancer::continueRoundingUntilReduction(int lower, int upper) {

    _lower_remainder_idx = lower;
    _upper_remainder_idx = upper;

    int idx = (_lower_remainder_idx+_upper_remainder_idx)/2;
    
    int localSum = 0;
    if (idx <= _remainders.size()) {
        // Remainder is either one of the remainders from the reduced sequence
        // or the right-hand limit 1.0
        double remainder = (idx < _remainders.size() ? _remainders[idx] : 1.0);
        // Round your local assignments and calculate utilization sum
        _rounded_assignments = getRoundedAssignments(_assignments, remainder, localSum);
    }

    iAllReduce(localSum);
    return false;
}

bool CutoffPriorityBalancer::continueRoundingFromReduction() {

    int rank = MyMpi::rank(MPI_COMM_WORLD);
    _rounding_iterations++;

    float utilization = _reduce_result;
    float diffToOptimum = _load_factor*MyMpi::size(_comm) - utilization;

    int idx = (_lower_remainder_idx+_upper_remainder_idx)/2;

    // Store result, if it is the best one so far
    if (_best_remainder_idx == -1 || std::abs(diffToOptimum) < std::abs(_best_utilization_diff)) {
        _best_utilization_diff = diffToOptimum;
        _best_remainder_idx = idx;
        _best_utilization = utilization;
    }

    // Termination?
    if (utilization == _last_utilization) { // Utilization unchanged?
        // Finished!
        if (!_remainders.isEmpty() && _best_remainder_idx <= _remainders.size()) {
            // Remainders are known to this node: apply and report
            int sum = 0;
            double remainder = (_best_remainder_idx < _remainders.size() ? _remainders[_best_remainder_idx] : 1.0);
            _rounded_assignments = getRoundedAssignments(_assignments, remainder, sum);
            for (auto it : _rounded_assignments) {
                _assignments[it.first] = it.second;
            }
            Console::log(Console::VVERB, 
                        "ROUNDING DONE its=%i rmd=%.3f util=%.2f err=%.2f", 
                        _rounding_iterations, remainder, _best_utilization, _best_utilization_diff);
        } else {
            // Remainders are unknown to this node (not needed)
            Console::log(Console::VVVVERB, 
                        "ROUNDING DONE its=%i err=%.2f", 
                        _rounding_iterations, _best_utilization_diff);
        }
        // reset to original state
        _best_remainder_idx = -1;
        _rounding_iterations = 0; 
        return true; // Balancing completely done

    } else {
        // Log iteration
        if (!_remainders.isEmpty() && idx <= _remainders.size()) {
            double remainder = (_best_remainder_idx < _remainders.size() ? _remainders[_best_remainder_idx] : 1.0);
            Console::log(Console::VVERB, "ROUNDING it=%i rmd=%.3f util=%.2f err=%.2f", 
                            _rounding_iterations, remainder, utilization, diffToOptimum);
        } else {
            Console::log(Console::VVVVERB, "ROUNDING it=%i rmd=? util=%.2f err=%.2f", 
                            _rounding_iterations, utilization, diffToOptimum);
        }
        _last_utilization = utilization;
    }

    if (_lower_remainder_idx < _upper_remainder_idx) {
        if (utilization < _load_factor*MyMpi::size(_comm)) {
            // Too few resources utilized
            _upper_remainder_idx = idx-1;
        }
        if (utilization > _load_factor*MyMpi::size(_comm)) {
            // Too many resources utilized
            _lower_remainder_idx = idx+1;
        }
    }
    
    return continueRoundingUntilReduction(_lower_remainder_idx, _upper_remainder_idx);
}

std::map<int, int> CutoffPriorityBalancer::getBalancingResult() {

    // Convert float assignments into actual integer volumes, store and return them
    std::map<int, int> volumes;
    for (auto it = _assignments.begin(); it != _assignments.end(); ++it) {
        int jobId = it->first;
        float assignment = std::max(1.0f, it->second);
        int intAssignment = Random::roundProbabilistically(assignment);
        volumes[jobId] = intAssignment;
        if (intAssignment != (int)assignment) {
            Console::log(Console::VVERB, " #%i : final assignment %.3f ~> %i", jobId, _assignments[jobId], intAssignment);
        } else {
            Console::log(Console::VVERB, " #%i : final assignment %i", jobId, intAssignment);
        }
    }
    for (auto it = volumes.begin(); it != volumes.end(); ++it) {
        updateVolume(it->first, it->second);
    }

    _balancing = false;
    delete _local_jobs;
    _local_jobs = NULL;
    return volumes;
}
