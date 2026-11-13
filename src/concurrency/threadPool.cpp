#include "threadPool.h"

using namespace std;

ThreadPool::ThreadPool(int threadCount) {
    if (threadCount < 1) {
        threadCount = 1;
    }

    for (int i = 0; i < threadCount; i++) {
        workers.emplace_back(&ThreadPool::workerLoop, this);
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::workerLoop() {
    while (true) {
        function<void()> task;
        {
            unique_lock<mutex> lock(queueMutex);

            taskAvailable.wait(lock, [this]() {
                return stopping || !tasks.empty();
            });

            if (stopping && tasks.empty()) {
                return;
            }

            task = move(tasks.front());
            tasks.pop();
            activeWorkers++;
        }

        try {
            task();
            completedTasks++;
        } catch (...) {
            failedTasks++;
        }

        {
            lock_guard<mutex> lock(queueMutex);
            activeWorkers--;

            if (tasks.empty() && activeWorkers == 0) {
                allDone.notify_all();
            }
        }
    }
}

bool ThreadPool::submit(function<void()> task) {
    {
        lock_guard<mutex> lock(queueMutex);

        if (stopping) {
            return false;
        }
        tasks.push(move(task));
    }

    taskAvailable.notify_one();
    return true;
}

void ThreadPool::waitIdle() {
    unique_lock<mutex> lock(queueMutex);

    allDone.wait(lock, [this]() {
        return tasks.empty() && activeWorkers == 0;
    });
}

void ThreadPool::shutdown() {
    {
        lock_guard<mutex> lock(queueMutex);
        stopping = true;
    }

    taskAvailable.notify_all();

    for (thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

int ThreadPool::threadCount() const {
    return (int)workers.size();
}

int ThreadPool::completedCount() const {
    return completedTasks;
}

int ThreadPool::failedCount() const {
    return failedTasks;
}
