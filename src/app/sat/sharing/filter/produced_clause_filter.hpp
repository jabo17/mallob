
#pragma once

#include <array>

#include "util/tsl/robin_map.h"
#include "../../data/produced_clause.hpp"
#include "../../data/produced_clause_candidate.hpp"
#include "util/sys/threading.hpp"
#include "../store/adaptive_clause_database.hpp"
#include "util/params.hpp"
#include "produced_clause_filter_commons.hpp"

// Packed struct to get in all meta data for a produced clause.
struct __attribute__ ((packed)) ClauseInfoWithLbd {

    // Best LBD so far this clause was PRODUCED (+inserted into buffer) with
    uint32_t minProducedLbd:5;
    // Best LBD so far this clause was SHARED to all solvers with
    uint32_t minSharedLbd:5;
    // Epoch of last modification (production, or sharing:=true)
    uint32_t lastSharedEpoch:22;

    // Bitset of which local solver(s) exported the clause
    cls_producers_bitset producers:MALLOB_MAX_N_APPTHREADS_PER_PROCESS;

    ClauseInfoWithLbd() {
        minProducedLbd = 0;
        minSharedLbd = 0;
        producers = 0;
        lastSharedEpoch = 0;
    }
    ClauseInfoWithLbd(const ProducedClauseCandidate& c) {
        minProducedLbd = c.lbd;
        minSharedLbd = 0;
        assert(c.producerId < MALLOB_MAX_N_APPTHREADS_PER_PROCESS);
        producers = 1 << c.producerId;
        lastSharedEpoch = 0;
    }
};

// Exact data structure which remembers clauses which were successfully exported by a solver.
// For each incoming clause, the structure can then be used to decide (a) if the clause should 
// be discarded ("filtered") because it was shared before (or too recently) and (b) which
// subset of solvers should receive the clauses (because they did not export it themselves).
// The structure takes space linear in the number of clauses successfully added to the
// AdaptiveClauseDatabase instance which is used for tryRegisterAndInsert. 
class ProducedClauseFilter {

template <typename T>
using ProducedMap = tsl::robin_map<T, ClauseInfoWithLbd, ProducedClauseHasher<T>, ProducedClauseEqualsCommutative<T>>;

private:
    ProducedMap<ProducedUnitClause> _map_units;
    ProducedMap<ProducedBinaryClause> _map_binaries;
    ProducedMap<ProducedLargeClause> _map_large_clauses;

    Mutex _map_mutex;

    const int _epoch_horizon;
    const bool _reshare_improved_lbd;

    ClauseInfoWithLbd _empty_clause_info;

public:
    ProducedClauseFilter(int epochHorizon, bool reshareImprovedLbd) : 
        _epoch_horizon(epochHorizon), _reshare_improved_lbd(reshareImprovedLbd) {}

    enum ExportResult {ADMITTED, FILTERED, DROPPED};
    ExportResult tryRegisterAndInsert(ProducedClauseCandidate&& c, AdaptiveClauseDatabase& cdb) {

        if (c.size == 1) {
            ProducedUnitClause pc;
            pc.literal = *c.begin;
            return tryRegisterAndInsert(pc, c, _map_units, cdb);

        } else if (c.size == 2) {
            ProducedBinaryClause pc;
            pc.literals[0] = std::min(c.begin[0], c.begin[1]);
            pc.literals[1] = std::max(c.begin[0], c.begin[1]);
            return tryRegisterAndInsert(pc, c, _map_binaries, cdb);

        } else {
            ProducedLargeClause pc;
            pc.size = c.size;
            pc.data = c.releaseData();
            return tryRegisterAndInsert(pc, c, _map_large_clauses, cdb);
        }
    }

    cls_producers_bitset getProducers(Mallob::Clause& c, int epoch) {

        if (c.size == 1) {
            ProducedUnitClause pc(c);
            return getProducers(pc, _map_units, epoch);

        } else if (c.size == 2) {
            ProducedBinaryClause pc(c);
            return getProducers(pc, _map_binaries, epoch);

        } else {
            ProducedLargeClause pc;
            pc.size = c.size;
            pc.data = c.begin;
            auto info = getProducers(pc, _map_large_clauses, epoch);
            pc.data = nullptr;
            return info;
        }
    }

    bool admitSharing(Mallob::Clause& c, int epoch) {

        if (c.size == 1) {
            ProducedUnitClause pc(c);
            return admitSharing(pc, _map_units, c.lbd, epoch);

        } else if (c.size == 2) {
            ProducedBinaryClause pc(c);
            return admitSharing(pc, _map_binaries, c.lbd, epoch);

        } else {
            ProducedLargeClause pc;
            pc.data = c.begin;
            pc.size = c.size;
            bool admitted = admitSharing(pc, _map_large_clauses, c.lbd, epoch);
            pc.data = nullptr; // avoid freeing of clause data reference
            return admitted;
        }
    }

    void erase(ProducedClauseCandidate& c) {

        if (c.size == 1) {
            ProducedUnitClause pc;
            pc.literal = *c.begin;
            _map_units.erase(pc);

        } else if (c.size == 2) {
            ProducedBinaryClause pc;
            pc.literals[0] = std::min(c.begin[0], c.begin[1]);
            pc.literals[1] = std::max(c.begin[0], c.begin[1]);
            _map_binaries.erase(pc);

        } else {
            ProducedLargeClause pc;
            pc.size = c.size;
            pc.data = c.releaseData();
            _map_large_clauses.erase(pc);
        }
    }

    inline bool tryAcquireLock() {return _map_mutex.tryLock();}
    inline void acquireLock() {_map_mutex.lock();}
    inline void releaseLock() {_map_mutex.unlock();}

    size_t size() const {
        return _map_units.size() + _map_binaries.size() + _map_large_clauses.size();
    }

private:
    template<typename T>
    ExportResult tryRegisterAndInsert(T& pc, ProducedClauseCandidate& c, ProducedMap<T>& map, AdaptiveClauseDatabase& cdb) {

        // Try to find clause
        auto it = map.find(pc);

        // If clause is contained:
        bool contained = it != map.end();
        if (contained) {
            int oldLbd = it.value().minProducedLbd;
            // No resharing upon improved LBD, or LBD not improved?
            // => Filter clause.
            if (!_reshare_improved_lbd || (oldLbd > 0 && c.lbd >= oldLbd)) {
                updateClauseInfo(c, it.value(), /*updateLbd=*/false);
                return FILTERED;
            }
            // Clause can be accepted (again) due to improved LBD score
        }

        // Try to insert to sharing database
        if (!cdb.addClause(prod_cls::data(pc), c.size, c.lbd)) {
            // No space left in database: update meta data, drop clause
            // (Do not update LBD value because the clause was not exported)
            if (contained) updateClauseInfo(c, it.value(), /*updateLbd=*/false);
            return DROPPED;
        }

        // Inserted: do register and set epoch to current epoch
        if (contained) updateClauseInfo(c, it.value(), /*updateLbd=*/true);
        else map.insert({std::move(pc), ClauseInfoWithLbd(c)});
        return ADMITTED;
    }

    void updateClauseInfo(const ProducedClauseCandidate& c, ClauseInfoWithLbd& info, bool updateLbd) {
        assert(c.lbd > 0);
        if (updateLbd) {
            if (info.minProducedLbd == 0 || info.minProducedLbd > c.lbd) {
                // Improved (or first) LBD
                info.minProducedLbd = c.lbd;
            }
        }
        // Add producing solver as a producer
        assert(c.producerId < MALLOB_MAX_N_APPTHREADS_PER_PROCESS);
        info.producers |= (1 << c.producerId);
    }

    template <typename T>
    inline bool admitSharing(const T& pc, ProducedMap<T>& map, int lbd, int epoch) {
        
        auto it = map.find(pc);
        if (it == map.end()) return true; // No entry? -> Admit trivially
        
        // There is a present entry for this clause
        ClauseInfoWithLbd& info = it.value();
        if (info.minSharedLbd > 0) {
            // Clause was shared before
            if (epoch - info.lastSharedEpoch <= _epoch_horizon) {
                // Clause was shared at some recent point in time
                if (!_reshare_improved_lbd) {
                    // Never reshare recent clauses, even with improved LBD
                    return false;
                }
                if (info.minSharedLbd <= lbd) {
                    // Clause was shared with this LBD or better: filter
                    return false; 
                }
            }
        }

        // Admit for sharing, update meta data to reflect sharing
        info.minSharedLbd = lbd;
        info.lastSharedEpoch = epoch;
        return true;
    }

    template <typename T>
    inline cls_producers_bitset getProducers(const T& pc, ProducedMap<T>& map, int epoch) {
        auto it = map.find(pc);
        if (it == map.end()) return 0;
        ClauseInfoWithLbd& info = it.value();
        return info.producers;
    }
};
