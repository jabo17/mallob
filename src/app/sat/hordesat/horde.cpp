/*
 * HordeLib.cpp
 *
 *  Created on: Mar 24, 2017
 *      Author: balyo
 */

#include "horde.hpp"

#include "utilities/debug_utils.hpp"
#include "utilities/default_logging_interface.hpp"
#include "sharing/default_sharing_manager.hpp"
#include "solvers/cadical.hpp"
#include "solvers/lingeling.hpp"
#ifdef MALLOB_USE_RESTRICTED
#include "solvers/glucose.hpp"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <csignal>
#include <unistd.h>
#include <sched.h>
#include <assert.h>

using namespace SolvingStates;

HordeLib::HordeLib(const Parameters& params, std::shared_ptr<LoggingInterface> loggingInterface) : 
			_params(params), _logger(loggingInterface), _state(INITIALIZING) {
	
    int appRank = params.getIntParam("apprank");
    int mpiSize = params.getIntParam("mpisize");

	hlog(0, "Hlib engine on job %s", params.getParam("jobstr").c_str());
	//params.printParams();
	_num_solvers = params.getIntParam("t", 1);
	_sleep_microsecs = 1000 * params.getIntParam("i", 1000);

	// Retrieve the string defining the cycle of solver choices, one character per solver
	// e.g. "llgc" => lingeling lingeling glucose cadical lingeling lingeling glucose ...
	std::string solverChoices = params.getParam("satsolver", "l");
	
	// These numbers become the diversifier indices of the solvers on this node
	int numLgl = 0;
	int numGlu = 0;
	int numCdc = 0;

	// Add solvers from full cycles on previous ranks
	// and from the begun cycle on the previous rank
	int numFullCycles = (appRank * _num_solvers) / solverChoices.size();
	int begunCyclePos = (appRank * _num_solvers) % solverChoices.size();
	for (size_t i = 0; i < solverChoices.size(); i++) {
		int* solverToAdd;
		switch (solverChoices[i]) {
		case 'l': solverToAdd = &numLgl; break;
		case 'g': solverToAdd = &numGlu; break;
		case 'c': solverToAdd = &numCdc; break;
		}
		*solverToAdd += numFullCycles + (i < begunCyclePos);
	}

	// Solver-agnostic options each solver in the portfolio will receive
	SolverSetup setup;
	setup.logger = _logger.get();
	setup.jobname = params.getParam("jobstr");
	setup.useAdditionalDiversification = params.isNotNull("aod");
	setup.hardInitialMaxLbd = params.getIntParam("ihlbd");
	setup.hardFinalMaxLbd = params.getIntParam("fhlbd");
	setup.softInitialMaxLbd = params.getIntParam("islbd");
	setup.softFinalMaxLbd = params.getIntParam("fslbd");
	setup.hardMaxClauseLength = params.getIntParam("hmcl");
	setup.softMaxClauseLength = params.getIntParam("smcl");

	// Instantiate solvers according to the global solver IDs and diversification indices
	int cyclePos = begunCyclePos;
	for (setup.localId = 0; setup.localId < _num_solvers; setup.localId++) {
		setup.globalId = appRank * _num_solvers + setup.localId;
		// Which solver?
		switch (solverChoices[cyclePos]) {
		case 'l':
			// Lingeling
			setup.diversificationIndex = numLgl++;
			hlog(3, "S%i : Lingeling-%i\n", setup.globalId, setup.diversificationIndex);
			_solver_interfaces.emplace_back(new Lingeling(setup));
			break;
		case 'c':
			// Cadical
			setup.diversificationIndex = numCdc++;
			hlog(3, "S%i : Cadical-%i\n", setup.globalId, setup.diversificationIndex);
			_solver_interfaces.emplace_back(new Cadical(setup));
			break;
#ifdef MALLOB_USE_RESTRICTED
		case 'g':
			// Glucose
			setup.diversificationIndex = numGlu++;
			hlog(3, "S%i: Glucose-%i\n", setup.globalId, setup.diversificationIndex);
			_solver_interfaces.emplace_back(new MGlucose(setup));
			break;
#endif
		default:
			// Invalid solver
			hlog(0, "Fatal error: Invalid solver \"%c\" assigned\n", solverChoices[cyclePos]);
			abort();
			break;
		}
		cyclePos = (cyclePos+1) % solverChoices.size();
	}

	_sharing_manager.reset(new DefaultSharingManager(_solver_interfaces, _params, *_logger));
	hlog(3, "initialized\n");
}

void HordeLib::beginSolving(const std::vector<std::shared_ptr<std::vector<int>>>& formulae, 
							const std::shared_ptr<std::vector<int>>& assumptions) {
	
	// Store payload to solve
	for (auto vec : formulae) {
		if (vec == NULL) return;
		_formulae.push_back(vec);
	}
	if (assumptions != NULL) _assumptions = assumptions;
	assert(_assumptions != NULL);

	_result = UNKNOWN;

	for (size_t i = 0; i < _num_solvers; i++) {
		_solver_threads.emplace_back(new SolverThread(
			_params, *_logger, _solver_interfaces[i], formulae, assumptions, i, &_solution_found
		));
		_solver_threads.back()->start();
	}

	setSolvingState(ACTIVE);
	hlog(3, "started solver threads\n");
}

void HordeLib::continueSolving(const std::vector<std::shared_ptr<std::vector<int>>>& formulae, 
								const std::shared_ptr<std::vector<int>>& assumptions) {
	
	// Update payload
	for (auto vec : formulae) _formulae.push_back(vec);
	_assumptions = assumptions;
	_result = UNKNOWN;

	// unset standby
	setSolvingState(ACTIVE);
}

void HordeLib::updateRole(int rank, int numNodes) {
	
}

bool HordeLib::isFullyInitialized() {
	if (_state == INITIALIZING) return false;
	for (size_t i = 0; i < _solver_threads.size(); i++) {
		if (!_solver_threads[i]->isInitialized()) return false;
	}
	return true;
}

int HordeLib::solveLoop() {
	if (isCleanedUp()) return -1;

	// Sleeping?
    if (_sleep_microsecs > 0) usleep(_sleep_microsecs);

    // Solving done?
	bool done = false;
	for (size_t i = 0; i < _solver_threads.size(); i++) {
		if (_solver_threads[i]->getState() == STANDBY) {
			done = true;
			_result = _solver_threads[i]->getSatResult();
			if (_result == SAT) {
				_model = _solver_threads[i]->getSolution();
			} else {
				_failed_assumptions = _solver_threads[i]->getFailedAssumptions();
			}
		}
	}

	if (done) {
		hlog(3, "Returning result\n");
		return _result;
	}
    return -1; // no result yet
}

int HordeLib::prepareSharing(int* begin, int maxSize) {
	if (isCleanedUp()) return 0;
	hlog(3, "collecting clauses on this node\n");
	return _sharing_manager->prepareSharing(begin, maxSize);
}

void HordeLib::digestSharing(const std::vector<int>& result) {
	if (isCleanedUp()) return;
	_sharing_manager->digestSharing(result);
}

void HordeLib::digestSharing(int* begin, int size) {
	if (isCleanedUp()) return;
	_sharing_manager->digestSharing(begin, size);
}

void HordeLib::dumpStats(bool final) {
	if (isCleanedUp() || !isFullyInitialized()) return;

	// Local statistics
	SolvingStatistics locSolveStats;
	for (size_t i = 0; i < _num_solvers; i++) {
		SolvingStatistics st = _solver_interfaces[i]->getStatistics();
		hlog(0, "%sS%d pps:%lu decs:%lu cnfs:%lu mem:%0.2f\n",
				final ? "END " : "",
				_solver_interfaces[i]->getGlobalId(), st.propagations, st.decisions, st.conflicts, st.memPeak);
		locSolveStats.conflicts += st.conflicts;
		locSolveStats.decisions += st.decisions;
		locSolveStats.memPeak += st.memPeak;
		locSolveStats.propagations += st.propagations;
		locSolveStats.restarts += st.restarts;
	}
	SharingStatistics locShareStats;
	if (_sharing_manager != NULL) locShareStats = _sharing_manager->getStatistics();

	unsigned long exportedWithFailed = locShareStats.exportedClauses + locShareStats.clausesFilteredAtExport + locShareStats.clausesDroppedAtExport;
	unsigned long importedWithFailed = locShareStats.importedClauses + locShareStats.clausesFilteredAtImport;
	hlog(0, "%spps:%lu decs:%lu cnfs:%lu mem:%0.2f exp:%lu/%lu(drp:%lu) imp:%lu/%lu\n",
			final ? "END " : "",
			locSolveStats.propagations, locSolveStats.decisions, locSolveStats.conflicts, locSolveStats.memPeak, 
			locShareStats.exportedClauses, exportedWithFailed, locShareStats.clausesDroppedAtExport, 
			locShareStats.importedClauses, importedWithFailed);

	if (final) {
		// Histogram over clause lengths (do not print trailing zeroes)
		std::string hist = "";
		std::string histZeroesOnly = "";
		for (size_t i = 1; i < CLAUSE_LEN_HIST_LENGTH; i++) {
			auto val = locShareStats.seenClauseLenHistogram[i];
			if (val > 0) {
				if (!histZeroesOnly.empty()) {
					// Flush sequence of zeroes into main string
					hist += histZeroesOnly;
					histZeroesOnly = "";
				}
				hist += " " + std::to_string(val);
			} else {
				// Append zero to side string
				histZeroesOnly += " " + std::to_string(val);
			}
		}
		if (!hist.empty()) hlog(0, "END clenhist:%s\n", hist.c_str());
	}
}

void HordeLib::setPaused() {
	if (_state == ACTIVE)	setSolvingState(SUSPENDED);
}

void HordeLib::unsetPaused() {
	if (_state == SUSPENDED) setSolvingState(ACTIVE);
}

void HordeLib::interrupt() {
	if (_state != STANDBY) {
		dumpStats(/*final=*/true);
		setSolvingState(STANDBY);
	}
}

void HordeLib::abort() {
	if (_state != ABORTING) setSolvingState(ABORTING);
}

void HordeLib::setSolvingState(SolvingState state) {
	SolvingState oldState = _state;
	_state = state;

	hlog(2, "state change %s -> %s\n", SolvingStateNames[oldState], SolvingStateNames[state]);
	for (auto& solver : _solver_threads) solver->setState(state);
}

int HordeLib::value(int lit) {
	return _model[abs(lit)];
}

int HordeLib::failed(int lit) {
	return _failed_assumptions.find(lit) != _failed_assumptions.end();
}

void HordeLib::hlog(int verbosityLevel, const char* fmt, ...) {
	va_list vl;
    va_start(vl, fmt);
	_logger->log_va_list(verbosityLevel, fmt, vl);
	va_end(vl);
}

void HordeLib::cleanUp() {
	double time = _logger->getTime();

	hlog(3, "[hlib-cleanup] enter\n");

	// for any running threads left:
	setSolvingState(ABORTING);
	
	// join and delete threads
	for (size_t i = 0; i < _solver_threads.size(); i++) {
		_solver_threads[i]->tryJoin();
	}
	_solver_threads.clear();
	hlog(3, "[hlib-cleanup] joined threads\n");

	// delete solvers
	_solver_interfaces.clear();
	hlog(3, "[hlib-cleanup] cleared solvers\n");

	time = _logger->getTime() - time;
	hlog(2, "[hlib-cleanup] done, took %.3f s\n", time);

	_cleaned_up = true;
}
