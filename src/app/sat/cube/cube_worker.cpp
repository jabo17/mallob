#include "cube_worker.hpp"

#include <cassert>

#include "app/sat/hordesat/solvers/cadical.hpp"
#include "app/sat/hordesat/solvers/cadical_interface.hpp"
#include "app/sat/console_horde_interface.hpp"
#include "cube_communicator.hpp"
#include "util/console.hpp"

CubeWorker::CubeWorker(std::vector<int> &formula, CubeCommunicator &cube_comm, LoggingInterface &logger, SatResult &result)
    : CubeWorkerInterface(formula, cube_comm, logger, result) {

    // Initialize solver
    SolverSetup setup;
    setup.logger = &_logger;
    _solver = std::make_unique<Cadical>(setup);

    // Read formula
    for (int lit : _formula) {
        _solver->addLiteral(lit);
    }
}

void CubeWorker::mainLoop() {
    auto lock = _state_mutex.getLock();

    assert(_worker_state == IDLING);

    _worker_state = WAITING;

    // TODO: Check if this works. Otherwise extend CondVar.wait to take a unique lock or use std::mutex

    while (true) {
        // After the condition is fulfilled, the lock is reaquired
        _state_cond.wait(lock, [&] { return _worker_state == WORKING || _isInterrupted; });

        _logger.log(0, "The main loop continues.\n");

        // Exit main loop
        if (_isInterrupted) {
            _logger.log(0, "Exiting main loop.\n");
            return;
        }

        // There should be local cubes available now
        assert(!_local_cubes.empty());

        // Start solving the local cubes
        SatResult result = solve();

        if (result == SAT) {
            _worker_state = SOLVED;
            _result = SAT;

        } else if (result == UNSAT) {
            _worker_state = FAILED;
        }
    }
}

SatResult CubeWorker::solve() {
    for (Cube &next_local_cube : _local_cubes) {
        auto result = _solver->solve(next_local_cube.getPath());

        // Check result
        if (result == SAT) {
            _logger.log(1, "Found a solution.\n");
            return SAT;

        } else if (result == UNKNOWN) {
            _logger.log(1, "Solving interrupted.\n");
            return UNKNOWN;

        } else if (result == UNSAT) {
            _logger.log(1, "Cube failed.\n");
            next_local_cube.fail();
        }
    }
    // All cubes were unsatisfiable
    return UNSAT;
}

void CubeWorker::startWorking() {
    _worker_thread = std::thread(&CubeWorker::mainLoop, this);
}

void CubeWorker::interrupt() {
    _isInterrupted.store(true);
    // Exit solve if currently solving
    _solver->interrupt();
    // Resume worker thread if currently waiting
    _state_cond.notify();
    // This guarantees termination of the mainLoop
}

void CubeWorker::join() {
    _worker_thread.join();
}

void CubeWorker::suspend() {
    _solver->suspend();
}

void CubeWorker::resume() {
    _solver->resume();
}

bool CubeWorker::wantsToCommunicate() {
    if (_worker_state == WAITING) {
        // Worker is waiting to request new cubes
        return true;

    } else if (_worker_state == FAILED) {
        // Worker is waiting to send his failed cubes
        return true;

    } else {
        // Default case
        return false;
    }
}

void CubeWorker::beginCommunication() {
    // Blocks until lock is aquired
    const std::lock_guard<Mutex> lock(_state_mutex);

    if (_worker_state == WAITING) {
        _worker_state = REQUESTING;
        _cube_comm.requestCubes();

    } else if (_worker_state == FAILED) {
        _worker_state = RETURNING;

        auto serialized_failed_cubes = serializeCubes(_local_cubes);
        _cube_comm.returnFailedCubes(serialized_failed_cubes);
    }
}

void CubeWorker::handleMessage(int source, JobMessage &msg) {
    if (msg.tag == MSG_SEND_CUBES) {
        auto serialized_cubes = msg.payload;
        auto cubes = unserializeCubes(serialized_cubes);
        digestSendCubes(cubes);

    } else if (msg.tag == MSG_RECEIVED_FAILED_CUBES) {
        digestReveicedFailedCubes();

    } else {
        // TODO: Throw error
    }
}

void CubeWorker::digestSendCubes(std::vector<Cube> cubes) {
    const std::lock_guard<Mutex> lock(_state_mutex);
    assert(_worker_state == REQUESTING);

    _logger.log(0, "Digesting send cubes.\n");

    _local_cubes = cubes;

    // Cubes were digested
    // Worker can now work
    _worker_state = WORKING;
    _state_cond.notify();
}

void CubeWorker::digestReveicedFailedCubes() {
    const std::lock_guard<Mutex> lock(_state_mutex);
    assert(_worker_state == RETURNING);

    _logger.log(0, "Digesting received failed cubes.\n");

    // Failed cubes were returned
    // Worker can now request new cubes
    _worker_state = WAITING;
}