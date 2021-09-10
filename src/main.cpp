
#include <iostream>
#include <set>
#include <stdlib.h>
#include <unistd.h>

#include "comm/mympi.hpp"
#include "util/sys/timer.hpp"
#include "util/logger.hpp"
#include "util/random.hpp"
#include "util/params.hpp"
#include "util/sys/shared_memory.hpp"
#include "util/sys/process.hpp"
#include "util/sys/proc.hpp"
#include "worker.hpp"
#include "client.hpp"
#include "util/sys/thread_pool.hpp"

#ifndef MALLOB_VERSION
#define MALLOB_VERSION "(dbg)"
#endif

void introduceMonoJob(Parameters& params, Client& client) {
    // Write a job JSON for the singular job to solve
    std::ofstream ofs(client.getApiPath() + "new/mono-job.json");
    ofs << "{ \"user\": \"admin\", \"name\": \"mono-job\", \"file\": \"";
    ofs << params.monoFilename();
    ofs << "\", \"priority\": 1.000 ";
    if (params.jobWallclockLimit() > 0)
        ofs << ", \"wallclock-limit\": \"" << params.jobWallclockLimit() << "s\" ";
    if (params.jobCpuLimit() > 0)
        ofs << ", \"cpu-limit\": \"" << params.jobCpuLimit() << "s\" ";
    ofs << "}";
}

void doMainProgram(MPI_Comm& commWorkers, MPI_Comm& commClients, Parameters& params) {

    // Determine which role(s) this PE has
    bool isWorker = commWorkers != MPI_COMM_NULL;
    bool isClient = commClients != MPI_COMM_NULL;
    if (isWorker) log(V4_VVER, "I am worker #%i\n", MyMpi::rank(commWorkers));
    if (isClient) log(V4_VVER, "I am client #%i\n", MyMpi::rank(commClients));

    // Create worker and client as necessary
    Worker* worker = isWorker ? new Worker(commWorkers, params) : nullptr;
    Client* client = isClient ? new Client(commClients, params) : nullptr;

    // If mono solving mode is enabled, introduce the singular job to solve
    if (params.monoFilename.isSet() && isClient && MyMpi::rank(commClients) == 0)
        introduceMonoJob(params, *client);
    
    // Initialize worker and client as necessary (background threads, callbacks, ...)
    if (isWorker) worker->init();
    if (isClient) client->init();
    
    // Register global callback for exiting msg (not specific to worker nor client)
    MyMpi::getMessageQueue().registerCallback(MSG_DO_EXIT, [](MessageHandle& h) {
        log(LOG_ADD_SRCRANK | V3_VERB, "Received exit signal", h.source);

        // Forward exit signal
        int myRank = MyMpi::rank(MPI_COMM_WORLD);
        if (myRank*2+1 < MyMpi::size(MPI_COMM_WORLD))
            MyMpi::isendCopy(myRank*2+1, MSG_DO_EXIT, h.getRecvData());
        if (myRank*2+2 < MyMpi::size(MPI_COMM_WORLD))
            MyMpi::isendCopy(myRank*2+2, MSG_DO_EXIT, h.getRecvData());

        Terminator::setTerminating();
    });

    log(V5_DEBG, "Global init barrier ...\n");
    MPI_Barrier(MPI_COMM_WORLD);
    log(V5_DEBG, "Passed global init barrier\n");

    // Main loop
    while (!Terminator::isTerminating(/*fromMainthread*/true)) {

        // Advance worker and client logic
        if (isWorker) worker->advance();
        if (isClient) client->advance();

        // Advance message queue and run callbacks for done messages
        MyMpi::getMessageQueue().advance();

        // Check termination, sleep, and/or yield thread
        if (isWorker && worker->checkTerminate(Timer::elapsedSeconds())) 
            break;
        if (params.sleepMicrosecs() > 0) usleep(params.sleepMicrosecs());
        if (params.yield()) std::this_thread::yield();
    }

    // Clean up
    if (isWorker) delete worker;
    if (isClient) delete client;
}

int main(int argc, char *argv[]) {
    
    Timer::init();
    MyMpi::init(argc, argv);

    int numNodes = MyMpi::size(MPI_COMM_WORLD);
    int rank = MyMpi::rank(MPI_COMM_WORLD);

    // Initialize bookkeeping of child processes and signals
    Process::init(rank);

    Parameters params;
    params.init(argc, argv);

    ProcessWideThreadPool::init(params.numThreadsPerProcess());

    bool quiet = params.quiet();
    if (params.zeroOnlyLogging() && rank > 0) quiet = true;
    std::string logdir = params.logDirectory();
    Logger::init(rank, params.verbosity(), params.coloredOutput(), 
            quiet, /*cPrefix=*/params.monoFilename.isSet(), 
            !logdir.empty() ? &logdir : nullptr);

    MyMpi::setOptions(params);

    if (rank == 0)
        params.printParams();
    if (params.help()) {
        // Help requested or no job input provided
        if (rank == 0) {
            params.printUsage();
        }
        MPI_Finalize();
        Process::doExit(0);
    }

    char hostname[1024];
	gethostname(hostname, 1024);
    log(V3_VERB, "Mallob %s pid=%lu on host %s\n", MALLOB_VERSION, Proc::getPid(), hostname);

    // Global and local seed, such that all nodes have access to a synchronized randomness
    // as well as to an individual randomness that differs among nodes
    Random::init(numNodes, rank);

    auto isWorker = [&](int rank) {
        if (params.numWorkers() == -1) return true; 
        return rank < params.numWorkers();
    };
    auto isClient = [&](int rank) {
        if (params.numClients() == -1) return true;
        return rank >= numNodes - params.numClients();
    };

    // Create communicators for clients and for workers
    std::vector<int> clientRanks;
    std::vector<int> workerRanks;
    for (int i = 0; i < numNodes; i++) {
        if (isWorker(i)) workerRanks.push_back(i);
        if (isClient(i)) clientRanks.push_back(i);
    }
    if (rank == 0) log(V3_VERB, "%i workers, %i clients\n", workerRanks.size(), clientRanks.size());
    
    MPI_Comm clientComm, workerComm;
    {
        MPI_Group worldGroup;
        MPI_Comm_group(MPI_COMM_WORLD, &worldGroup);
        MPI_Group clientGroup;
        MPI_Group_incl(worldGroup, clientRanks.size(), clientRanks.data(), &clientGroup);
        MPI_Comm_create(MPI_COMM_WORLD, clientGroup, &clientComm);
    }
    {
        MPI_Group worldGroup;
        MPI_Comm_group(MPI_COMM_WORLD, &worldGroup);
        MPI_Group workerGroup;
        MPI_Group_incl(worldGroup, workerRanks.size(), workerRanks.data(), &workerGroup);
        MPI_Comm_create(MPI_COMM_WORLD, workerGroup, &workerComm);
    }
    
    // Execute main program
    try {
        doMainProgram(workerComm, clientComm, params);
    } catch (const std::exception& ex) {
        log(V0_CRIT, "[ERROR] uncaught \"%s\"\n", ex.what());
        Process::doExit(1);
    } catch (...) {
        log(V0_CRIT, "[ERROR] uncaught exception\n");
        Process::doExit(1);
    }

    // Exit properly
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    log(V2_INFO, "Exiting happily\n");
    Process::doExit(0);
}
