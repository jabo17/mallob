
#pragma once

#include "app/sat/data/solver_statistics.hpp"
#include "app/sat/execution/solver_setup.hpp"
#include "app/sat/sharing/buffer/buffer_reader.hpp"
#include "app/sat/sharing/store/generic_clause_store.hpp"
#include "util/sys/threading.hpp"
#include <atomic>

class GenericImportManager {

protected:
    SolverStatistics& _stats;
    int _max_clause_length;
    bool _reset_lbd;
    bool _increment_lbd;

    int _imported_revision {0};
    int _solver_revision {0};
    Mutex _mtx_revision;

    int _nb_div_blocks {0};
    int _global_id;

public:
    GenericImportManager(const SolverSetup& setup, SolverStatistics& stats) : _stats(stats), 
        _max_clause_length(setup.strictClauseLengthLimit),
        _reset_lbd(setup.resetLbdBeforeImport),
        _increment_lbd(setup.incrementLbdBeforeImport) {
	if (setup.baselinePlus) {
		_nb_div_blocks = setup.globalId % 11;
		_global_id = setup.globalId;
	}
	}
    virtual ~GenericImportManager() {};

    virtual void addSingleClause(const Mallob::Clause& c) = 0;
    virtual void performImport(BufferReader& reader) = 0;
    void setImportedRevision(int revision) {
        auto lock = _mtx_revision.getLock();
        _imported_revision = revision;
    }
    void updateSolverRevision(int solverRevision) {
        auto lock = _mtx_revision.getLock();
        _solver_revision = solverRevision;
    }
    bool canImport() {
	if (_nb_div_blocks > 0 && !empty()) {
		_nb_div_blocks--;
		if (_nb_div_blocks == 0) LOG(V4_VVER, "S%i not import-blocked any longer\n", _global_id);
		return false;		
	}
        return _solver_revision >= _imported_revision;
    }
    virtual const std::vector<int>& getUnitsBuffer() = 0;
    virtual Mallob::Clause& get(GenericClauseStore::ExportMode mode) = 0;

    virtual bool empty() const {
        return size() == 0;
    }
    virtual size_t size() const = 0;

    int getLiteralBudget(const SolverSetup& setup) {
        return setup.clauseBaseBufferSize * std::max(
            setup.minNumChunksPerSolver, 
            (int) (
                ((float) setup.numBufferedClsGenerations) * 
                setup.anticipatedLitsToImportPerCycle / setup.clauseBaseBufferSize
            )
        );
    }
};
