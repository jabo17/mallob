
#ifndef DOMPASCH_MALLOB_OPTIONS_LIST_HPP
#define DOMPASCH_MALLOB_OPTIONS_LIST_HPP

#include "util/option.hpp"

// All declared options will be stored in this member of the Parameters class.
OptMap _map;

// New options can be added here. They will then be initialized as member fields in the Parameters class.
// The value of an option can be queried on a Parameters object _params as such:
// _params.optionmember()

//  TYPE  member name                    option ID (short, long)                      default (, min, max)     description

OPT_BOOL(collectClauseHistory,           "ch", "collect-clause-history",              false,                   "Employ clause history collection mechanism")
OPT_BOOL(coloredOutput,                  "colors", "",                                false,                   "Colored terminal output based on messages' verbosity")
OPT_BOOL(continuousGrowth,               "cg", "continuous-growth",                   true,                    "Continuous growth of job demands")
OPT_BOOL(delayMonkey,                    "delaymonkey", "",                           false,                   "Small chance for each MPI call to block for some random amount of time")
OPT_BOOL(derandomize,                    "derandomize", "",                           true,                    "Derandomize job bouncing and build a <bounce-alternatives>-regular message graph instead")
OPT_BOOL(explicitVolumeUpdates,          "evu", "explicit-volume-updates",            false,                   "Broadcast volume updates through job tree instead of letting each PE compute it itself")
OPT_BOOL(help,                           "h", "help",                                 false,                   "Print help and exit")
OPT_BOOL(useFilesystemInterface,         "interface-fs", "",                          true,                    "Use filesystem interface (.api/{in,out}/*.json)")
OPT_BOOL(useIPCSocketInterface,          "interface-ipc", "",                         false,                   "Use IPC socket interface (.mallob.<pid>.sk)")
OPT_BOOL(jitterJobPriorities,            "jjp", "jitter-job-priorities",              false,                   "Jitter job priorities to break ties during rebalancing")
OPT_BOOL(latencyMonkey,                  "latencymonkey", "",                         false,                   "Block all MPI_Isend operations by a small randomized amount of time")
OPT_BOOL(monitorMpi,                     "mmpi", "monitor-mpi",                       false,                   "Launch an additional thread per process checking when the main thread is inside an MPI call")
OPT_BOOL(phaseDiversification,           "phasediv", "",                              true,                    "Diversify solvers based on phase in addition to native diversification")
OPT_BOOL(pipeLargeSolutions,             "pls", "pipe-large-solutions",               true,                    "Provide large solutions over a named pipe instead of directly writing them into the response JSON")
OPT_BOOL(quiet,                          "q", "quiet",                                false,                   "Do not log to stdout besides critical information")
OPT_BOOL(reactivationScheduling,         "rs", "use-reactivation-scheduling",         true,                    "Perform reactivation-based scheduling")
OPT_BOOL(useChecksums,                   "checksums", "",                             false,                   "Compute and verify checksum for every job description transfer")
OPT_BOOL(warmup,                         "warmup", "",                                false,                   "Do one explicit All-To-All warmup among all nodes in the beginning")
OPT_BOOL(workRequests,                   "wr", "use-work-requests",                   false,                   "Send around work requests similarly to job requests")
OPT_BOOL(yield,                          "yield", "",                                 false,                   "Yield manager thread whenever there are no new messages")
OPT_BOOL(zeroOnlyLogging,                "0o", "zero-only-logging",                   false,                   "Only PE of rank zero does logging")
OPT_BOOL(abortNonincrementalSubprocess,  "ans", "abort-noninc-subproc",               false,                   "Abort (hence restart) each sub-process which works (partially) non-incrementally upon the arrival of a new revision")

OPT_INT(activeJobsPerClient,             "ajpc", "active-jobs-per-client",            0,         0, LARGE_INT, "Make each client have up to this many active jobs at any given time")
OPT_INT(bufferedImportedClsGenerations,  "bicg", "buffered-imported-cls-generations", 4,         1, LARGE_INT, "Number of subsequent full clause sharings to fit in each solver's import buffer")
OPT_INT(clauseBufferBaseSize,            "cbbs", "clause-buffer-base-size",           1500,      0, MAX_INT,   "Clause buffer base size in integers")
OPT_INT(clauseHistoryAggregationFactor,  "chaf", "clause-history-aggregation",        5,         1, LARGE_INT, "Aggregate historic clause batches by this factor")
OPT_INT(clauseHistoryShortTermMemSize,   "chstms", "clause-history-shortterm-size",   10,        1, LARGE_INT, "Save this many \"full\" aggregated epochs until reducing them")
OPT_INT(firstApiIndex,                   "fapii", "first-api-index",                  0,    0, LARGE_INT,      "1st API index: with c clients, uses .api/jobs.{<index>..<index>+c-1}/ as directories")
OPT_INT(hopsBetweenBfs,                  "hbbfs", "hops-between-bfs",                 10,   0, MAX_INT,        "After a job request hopped this many times after unsuccessful \"hill climbing\" BFS, perform another BFS")
OPT_INT(hopsUntilBfs,                    "hubfs", "hops-until-bfs",                   LARGE_INT, 0, MAX_INT,   "After a job request hopped this many times, perform a \"hill climbing\" BFS")
OPT_INT(hopsUntilCollectiveAssignment,   "huca", "hops-until-collective-assignment",  0,    -1, LARGE_INT,     "After a job request hopped this many times, add it to collective negotiation of requests and idle nodes (0: immediately, -1: never");
OPT_INT(jobCacheSize,                    "jc", "job-cache-size",                      4,    0, LARGE_INT,      "Size of job cache per PE for suspended yet unfinished job nodes")
OPT_INT(maxBfsDepth,                     "mbfsd", "max-bfs-depth",                    4,    0, LARGE_INT,      "Max. depth to explore with hill climbing BFS for job requests")
OPT_INT(maxDemand,                       "md", "max-demand",                          0,    0, LARGE_INT,      "Limit any job's demand to this value")
OPT_INT(maxIdleDistance,                 "mid", "max-idle-distance",                  0,    0, LARGE_INT,      "Propagate idle distance of workers up to this limit through worker graph to weight randomness in request bouncing")
OPT_INT(maxLbdPartitioningSize,          "mlbdps", "max-lbd-partition-size",          8,    1, LARGE_INT,      "Store clauses with up to this LBD in separate buckets")
OPT_INT(messageBatchingThreshold,        "mbt", "message-batching-threshold",         1000000, 1000, MAX_INT,  "Employ batching of messages in batches of provided size")
OPT_INT(minNumChunksForImportPerSolver,  "mcips", "min-import-chunks-per-solver",     10,   1, LARGE_INT,      "Min. number of cbbs-sized chunks for buffering produced clauses for export")
OPT_INT(numBounceAlternatives,           "ba", "bounce-alternatives",                 4,    1, LARGE_INT,      "Number of bounce alternatives per PE (only relevant if -derandomize)")
OPT_INT(numChunksForExport,              "nce", "export-chunks",                      20,   1, LARGE_INT,      "Number of cbbs-sized chunks for buffering produced clauses for export")
OPT_INT(numClients,                      "c", "clients",                              1,    -1, LARGE_INT,     "Number of client PEs to initialize (counting backwards from last rank), -1: all PEs are clients")
OPT_INT(numJobs,                         "J", "jobs",                                 0,    0, LARGE_INT,      "Exit as soon as this number of jobs has been processed")
OPT_INT(numThreadsPerProcess,            "t", "threads-per-process",                  1,    0, LARGE_INT,      "Number of worker threads per node")
OPT_INT(maxLiteralsPerThread,            "mlpt", "max-lits-per-thread",               50000000, 0, MAX_INT,    "If formula is larger than threshold, reduce #threads per PE until #threads=1 or until limit is met \"on average\"")
OPT_INT(qualityClauseLengthLimit,        "qcll", "quality-clause-length-limit",       8,    0, LARGE_INT,      "Clauses up to this length are considered \"high quality\"")
OPT_INT(qualityLbdLimit,                 "qlbdl", "quality-lbd-limit",                2,    0, LARGE_INT,      "Clauses with an LBD score up to this value are considered \"high quality\"")
OPT_INT(seed,                            "seed", "",                                  0,    0, MAX_INT,        "Random seed")
OPT_INT(sleepMicrosecs,                  "sleep", "",                                 100,  0, LARGE_INT,      "Sleep this many microseconds between loop cycles of worker main thread")
OPT_INT(strictClauseLengthLimit,         "scll", "strict-clause-length-limit",        30,   0, LARGE_INT,      "Only clauses up to this length will be shared")
OPT_INT(strictLbdLimit,                  "slbdl", "strict-lbd-limit",                 30,   0, LARGE_INT,      "Only clauses with an LBD score up to this value will be shared")
OPT_INT(verbosity,                       "v", "verbosity",                            2,    0, 6,              "Logging verbosity: 0=CRIT 1=WARN 2=INFO 3=VERB 4=VVERB 5=DEBG")
OPT_INT(numWorkers,                      "w", "workers",                              -1,   -1, LARGE_INT,     "Number of worker PEs to initialize (beginning from rank #0), -1: all PEs are workers")
OPT_INT(watchdogAbortMillis,             "wam", "watchdog-abort-millis",              10000, 1, MAX_INT,       "Interval (in milliseconds) after which an un-reset watchdog in a worker's main thread will invoke a crash")

OPT_FLOAT(appCommPeriod,                 "s", "app-comm-period",                      1,    0, LARGE_INT,      "Do job-internal communication every t seconds") 
OPT_FLOAT(balancingPeriod,               "p", "balancing-period",                     0.1,  0, LARGE_INT,      "Minimum interval between subsequent rounds of balancing")
OPT_FLOAT(clauseBufferDiscountFactor,    "cbdf", "clause-buffer-discount",            0.9,  0.5, 1,            "Clause buffer discount factor: reduce buffer size per PE by <factor> each depth")
OPT_FLOAT(clauseFilterClearInterval,     "cfci", "clause-filter-clear-interval",      20,   -1, LARGE_INT,     "Set clear interval of clauses in solver filters (-1: never clear, 0: always clear")
OPT_FLOAT(crashMonkeyProbability,        "cmp", "crash-monkey",                       0,    0, 1,              "Have a solver thread crash with this probability each time it imports a clause")
OPT_FLOAT(growthPeriod,                  "g", "growth-period",                        0,    0, LARGE_INT,      "Grow job demand exponentially every t seconds (0: immediate full growth)" )
OPT_FLOAT(inputShuffleProbability,       "isp", "input-shuffle-probability",          0,    0, 1,              "Probability for solver with exhausted diversification to shuffle all clauses and all literals of each clause in the input")
OPT_FLOAT(jobCommUpdatePeriod,           "jcup", "job-comm-update-period",            0,    0, LARGE_INT,      "Job communicator update period (0: never update)" )
OPT_FLOAT(jobCpuLimit,                   "jcl", "job-cpu-limit",                      0,    0, LARGE_INT,      "Timeout an instance after x cpu seconds")
OPT_FLOAT(jobWallclockLimit,             "jwl", "job-wallclock-limit",                0,    0, LARGE_INT,      "Timeout an instance after x seconds wall clock time")
OPT_FLOAT(loadFactor,                    "l", "load-factor",                          1,    0, 1,              "Load factor to be aimed at")
OPT_FLOAT(requestTimeout,                "rto", "request-timeout",                    0,    0, LARGE_INT,      "Request timeout: discard non-root job requests when older than this many seconds")
OPT_FLOAT(sysstatePeriod,                "y", "sysstate-period",                      1,    0.1, 50,           "Period for aggregating and logging global system state")
OPT_FLOAT(timeLimit,                     "T", "time-limit",                           0,    0, LARGE_INT,      "Run entire system for at most this many seconds")

OPT_STRING(applicationConfiguration,     "app-config", "",                            "",                      "Application configuration: structured as (-key=value;)*")
OPT_STRING(applicationSpawnMode,         "appmode", "app-spawn-mode",                 "fork",                  "Application mode: \"fork\" (spawn child process for each job on each MPI process) or \"thread\" (execute jobs in separate threads but within the same process)")
OPT_STRING(hordeConfig,                  "hConf", "horde-config",                     "",                      "Supply Horde config for solver subprocess [internal option, do not use]")
OPT_STRING(logDirectory,                 "log", "log-directory",                      "",                      "Directory to save logs in")
OPT_STRING(monoFilename,                 "mono", "",                                  "",                      "Mono instance: Solve the provided CNF instance with full power, then exit")
OPT_STRING(satSolverSequence,            "satsolver",  "",                            "L",                     "Sequence of SAT solvers to cycle through (capital letter for true incremental solver, lowercase for pseudo-incremental solving): L|l:Lingeling C|c:CaDiCaL G|g:Glucose k:Kissat m:MergeSAT")
OPT_STRING(solutionToFile,               "s2f", "solution-to-file",                   "",                      "Write solutions to file with provided base name + job ID")
OPT_STRING(subprocessPrefix,             "subproc-prefix", "",                        "",                      "Execute SAT subprocess with this prefix (e.g., \"valgrind\")")

#endif
