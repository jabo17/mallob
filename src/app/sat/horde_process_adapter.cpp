
#include <assert.h>
#include <sys/types.h>
#include <stdlib.h>

#include "horde_process_adapter.hpp"

#include "hordesat/horde.hpp"
#include "util/sys/shared_memory.hpp"
#include "util/sys/proc.hpp"
#include "util/sys/timer.hpp"
#include "util/sys/fork.hpp"
#include "util/console.hpp"

HordeProcessAdapter::HordeProcessAdapter(const Parameters& params,
            const std::vector<std::shared_ptr<std::vector<int>>>& formulae, const std::shared_ptr<std::vector<int>>& assumptions) :
                _params(params), _formulae(formulae), _assumptions(assumptions) {

    initSharedMemory();
}

void HordeProcessAdapter::initSharedMemory() {

    // Initialize "management" shared memory
    _shmem_id = "/edu.kit.mallob." + _params.getParam("jobname");
    void* mainShmem = SharedMemory::create(_shmem_id, sizeof(HordeSharedMemory));
    _shmem.push_back(std::tuple<std::string, void*, int>(_shmem_id, mainShmem, sizeof(HordeSharedMemory)));
    _hsm = new ((char*)mainShmem) HordeSharedMemory();
    _hsm->portfolioRank = _params.getIntParam("mpirank");
    _hsm->portfolioSize = _params.getIntParam("mpisize");
    _hsm->doExport = false;
    _hsm->doImport = false;
    _hsm->doDumpStats = false;
    _hsm->doUpdateRole = false;
    _hsm->doInterrupt = false;
    _hsm->exportBufferMaxSize = 0;
    _hsm->importBufferSize = 0;
    _hsm->didExport = false;
    _hsm->didImport = false;
    _hsm->didDumpStats = false;
    _hsm->didUpdateRole = false;
    _hsm->didInterrupt = false;
    _hsm->isSpawned = false;
    _hsm->isInitialized = false;
    _hsm->hasSolution = false;
    _hsm->result = UNKNOWN;
    _hsm->solutionSize = 0;
    _hsm->exportBufferTrueSize = 0;

    // Put formulae into their own blocks of shared memory
    int fIdx = 0;
    for (const auto& f : _formulae) {
        int size = sizeof(int) * f->size();
        std::string fShmemId = _shmem_id + ".formulae." + std::to_string(fIdx);
        void* fShmem = SharedMemory::create(fShmemId, size);
        _shmem.push_back(std::tuple<std::string, void*, int>(fShmemId, fShmem, size));
        memcpy((int*)fShmem, f->data(), size);
        _params.setParam("fbufsize" + std::to_string(fIdx), std::to_string(size));
    }

    // Put assumptions into their own block of shared memory
    int size = sizeof(int) * _assumptions->size();
    std::string aShmemId = _shmem_id + ".assumptions";
    void* aShmem = SharedMemory::create(aShmemId, size);
    _shmem.push_back(std::tuple<std::string, void*, int>(aShmemId, aShmem, size));
    memcpy((int*)aShmem, _assumptions->data(), size);
    _params.setParam("asmptbufsize", std::to_string(size));

    // Create block of shared memory for clause export
    int maxExportBufferSize = _params.getIntParam("cbbs") * sizeof(int);
    std::string exportShmemId = _shmem_id + ".clauseexport";
    _export_buffer = (int*) SharedMemory::create(exportShmemId, maxExportBufferSize);
    _shmem.push_back(std::tuple<std::string, void*, int>(exportShmemId, _export_buffer, maxExportBufferSize));
    memset(_export_buffer, 0, maxExportBufferSize);

    // Create block of shared memory for clause import
    int maxImportBufferSize = _params.getIntParam("cbbs") * sizeof(int) * _params.getIntParam("mpisize");
    std::string importShmemId = _shmem_id + ".clauseimport";
    _import_buffer = (int*) SharedMemory::create(importShmemId, maxImportBufferSize);
    _shmem.push_back(std::tuple<std::string, void*, int>(importShmemId, _import_buffer, maxImportBufferSize));
    memset(_import_buffer, 0, maxImportBufferSize);
}

HordeProcessAdapter::~HordeProcessAdapter() {
    for (auto& shmem : _shmem) {
        SharedMemory::free(std::get<0>(shmem), (char*)std::get<1>(shmem), std::get<2>(shmem));
    }
}

pid_t HordeProcessAdapter::run() {

    // Assemble c-style program arguments
    const char** argv = new const char*[_params.getMap().size()+1];
    std::string argstr = "";
    int i = 0;
    for (const auto& param : _params.getMap()) {
        _params.setParam(param.first, "-" + param.first + "=" + param.second);
        argv[i] = _params.getMap().at(param.first).c_str();
        argstr += " " + std::string(argv[i]);
        i++;
    }
    argv[i] = nullptr;
    const char* execname = "mallob_sat_process";

    setenv("PATH", ("build/app/sat:" + std::string((const char*) getenv("PATH"))).c_str(), /*overwrite=*/1);

    Console::log(Console::VERB, "Invoke fork(); %s %s\n", execname, argstr.c_str());

    // FORK: Create a child process
    pid_t res = Fork::createChild();
    if (res > 0) {
        // [parent process]
        _child_pid = res;
        //while (!_hsm->isSpawned) usleep(250 /* 1/4 milliseconds */);
        _state = SolvingStates::ACTIVE;
        return res;
    }

    // [child process]
    // Execute the SAT process.
    execvp(execname, (char* const*)argv);

    abort(); // if this is reached, something went wrong with execvp
}

bool HordeProcessAdapter::isFullyInitialized() {
    return _hsm->isInitialized;
}

pid_t HordeProcessAdapter::getPid() {
    return _child_pid;
}

void HordeProcessAdapter::setSolvingState(SolvingStates::SolvingState state) {
    if (state == _state) return;

    if (state == SolvingStates::ABORTING) {
        Fork::terminate(_child_pid); // Terminate child process.
    }
    if (state == SolvingStates::SUSPENDED) {
        Fork::suspend(_child_pid); // Stop (suspend) process.
    }
    if (state == SolvingStates::ACTIVE) {
        Fork::resume(_child_pid); // Continue (resume) process.
    }
    if (state == SolvingStates::STANDBY) {
        _hsm->doInterrupt = true;
    }

    _state = state;
}

void HordeProcessAdapter::updateRole(int rank, int size) {
    _hsm->portfolioRank = rank;
    _hsm->portfolioSize = size;
    _hsm->doUpdateRole = true;
}

void HordeProcessAdapter::collectClauses(int maxSize) {
    _hsm->exportBufferMaxSize = maxSize;
    _hsm->doExport = true;
    if (_hsm->isInitialized) Fork::wakeUp(_child_pid);
}
bool HordeProcessAdapter::hasCollectedClauses() {
    return _hsm->didExport;
}
std::vector<int> HordeProcessAdapter::getCollectedClauses() {
    if (!_hsm->didExport) {
        return std::vector<int>();
    }
    std::vector<int> clauses(_hsm->exportBufferTrueSize);
    memcpy(clauses.data(), _export_buffer, clauses.size()*sizeof(int));
    _hsm->doExport = false;
    return clauses;
}

void HordeProcessAdapter::digestClauses(const std::vector<int>& clauses) {
    _hsm->importBufferSize = clauses.size();
    memcpy(_import_buffer, clauses.data(), clauses.size()*sizeof(int));
    _hsm->doImport = true;
    if (_hsm->isInitialized) Fork::wakeUp(_child_pid);
}

void HordeProcessAdapter::dumpStats() {
    _hsm->doDumpStats = true;
    // No hard need to wake up immediately
}

bool HordeProcessAdapter::check() {
    if (_hsm->didImport)     _hsm->doImport     = false;
    if (_hsm->didUpdateRole) _hsm->doUpdateRole = false;
    if (_hsm->didInterrupt)  _hsm->doInterrupt  = false;
    if (_hsm->didDumpStats)  _hsm->doDumpStats  = false;
    return _hsm->hasSolution;
}

std::pair<SatResult, std::vector<int>> HordeProcessAdapter::getSolution() {
    if (_hsm->solutionSize == 0) return std::pair<SatResult, std::vector<int>>(_hsm->result, std::vector<int>()); 
    std::vector<int> solution(_hsm->solutionSize);
    int* shmemSolution = (int*) SharedMemory::access((_shmem_id + ".solution").c_str(), solution.size());
    memcpy(solution.data(), shmemSolution, solution.size()*sizeof(int));
    return std::pair<SatResult, std::vector<int>>(_hsm->result, solution);
}