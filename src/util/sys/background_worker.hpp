
#ifndef DOMPASCH_MALLOB_BACKGROUND_WORKER_HPP
#define DOMPASCH_MALLOB_BACKGROUND_WORKER_HPP

#include <thread>
#include <functional>

#include "util/sys/terminator.hpp"
#include "util/logger.hpp"
#include "util/sys/timer.hpp"

class BackgroundWorker {

private:
    bool _terminate = false;
    std::thread _thread;

public:
    BackgroundWorker() {}
    void run(std::function<void()> runnable) {
	    float time = Timer::elapsedSeconds();
        _terminate = false;
        _thread = std::thread(runnable);
        time = Timer::elapsedSeconds() - time;
        log(V5_DEBG, "starting bg worker took %.5fs\n");
    }
    bool continueRunning() const {
        return !Terminator::isTerminating() && !_terminate;
    }
    bool isRunning() const {
        return _thread.joinable();
    }
    void stop() {
        _terminate = true;
        if (_thread.joinable()) _thread.join();
    }
    ~BackgroundWorker() {
        stop();
    }
};

#endif
