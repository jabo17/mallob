
# SAT-specific sources
set(SAT_SOURCES src/app/sat/parse/sat_reader.cpp src/app/sat/execution/engine.cpp src/app/sat/execution/solver_thread.cpp src/app/sat/execution/solving_state.cpp src/app/sat/job/anytime_sat_clause_communicator.cpp src/app/sat/job/forked_sat_job.cpp src/app/sat/job/threaded_sat_job.cpp src/app/sat/job/sat_process_adapter.cpp src/app/sat/job/sat_process_config.cpp src/app/sat/job/historic_clause_storage.cpp src/app/sat/sharing/store/adaptive_clause_database.cpp src/app/sat/sharing/buffer/buffer_merger.cpp src/app/sat/sharing/buffer/buffer_reader.cpp src/app/sat/sharing/filter/clause_buffer_lbd_scrambler.cpp src/app/sat/sharing/sharing_manager.cpp src/app/sat/solvers/cadical.cpp src/app/sat/solvers/kissat.cpp src/app/sat/solvers/lingeling.cpp src/app/sat/solvers/portfolio_solver_interface.cpp src/app/sat/data/clause_metadata.cpp src/app/sat/proof/lrat_utils.cpp)

# Add SAT-specific sources to main Mallob executable
set(BASE_SOURCES ${BASE_SOURCES} ${SAT_SOURCES} CACHE INTERNAL "")

#message("commons+SAT sources: ${BASE_SOURCES}") # Use to debug

# Include default SAT solvers as external libraries (their Mallob-side interfaces are part of SAT_SOURCES)
link_directories(lib/lingeling lib/yalsat lib/kissat)
set(BASE_LIBS ${BASE_LIBS} lgl yals cadical kissat CACHE INTERNAL "")
set(BASE_INCLUDES ${BASE_INCLUDES} lib CACHE INTERNAL "") # need to include some solver code

# Special case Cadical (used for certified UNSAT)
if(MALLOB_CERTIFIED_UNSAT)
    link_directories(lib/lrat-cadical)
else()
    link_directories(lib/cadical)
endif()

# Add new non-default solvers here
if(MALLOB_USE_GLUCOSE)
    link_directories(lib/glucose)
    set(BASE_SOURCES ${BASE_SOURCES} src/app/sat/solvers/glucose.cpp CACHE INTERNAL "")
    set(BASE_LIBS ${BASE_LIBS} glucose CACHE INTERNAL "")
    set(BASE_INCLUDES ${BASE_INCLUDES} lib/glucose CACHE INTERNAL "")
    add_definitions(-DMALLOB_USE_GLUCOSE)
endif()    
if(MALLOB_USE_MERGESAT)
    link_directories(lib/mergesat)
    set(BASE_SOURCES ${BASE_SOURCES} src/app/sat/solvers/mergesat.cpp CACHE INTERNAL "")
    set(BASE_LIBS ${BASE_LIBS} mergesat CACHE INTERNAL "")
    add_definitions(-DMALLOB_USE_MERGESAT)
endif()
# Further solvers here ...

# Executable of SAT subprocess
add_executable(mallob_sat_process ${SAT_SOURCES} src/app/sat/main.cpp)
target_include_directories(mallob_sat_process PRIVATE ${BASE_INCLUDES})
target_compile_options(mallob_sat_process PRIVATE ${BASE_COMPILEFLAGS})
target_link_libraries(mallob_sat_process mallob_commons)

# Add unit tests
new_test(sat_reader)
new_test(clause_database)
new_test(import_buffer)
new_test(clause_filter)
new_test(variable_translator)
new_test(distributed_clause_filter)
new_test(job_description)
new_test(serialized_formula_parser)
new_test(distributed_file_merger)
new_test(lrat_utils)
new_test(priority_clause_buffer)
new_test(clause_store_iteration)
#new_test(historic_clause_storage)
