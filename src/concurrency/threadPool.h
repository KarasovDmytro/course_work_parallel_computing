#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queueMutex;
    std::condition_variable taskAvailable;
    std::condition_variable allDone;

    int activeWorkers = 0;
    bool stopping = false;

    std::atomic<int> completedTasks{0};
    std::atomic<int> failedTasks{0};

    void workerLoop();

public:
    ThreadPool(int threadCount);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    bool submit(std::function<void()> task);
    void waitIdle();
    void shutdown();

    int threadCount() const;
    int completedCount() const;
    int failedCount() const;
};
