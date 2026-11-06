#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

using Task = std::function<void()>;

class TaskQueue {
private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<Task> tasks_;
    bool stopping_ = false;

public:
    bool push(Task task);
    bool pop(Task &task);
    void stop();
};

class ThreadPool {
private:
    void worker_loop();
    TaskQueue queue_;
    
    std::vector<std::thread> workers_;
    std::atomic<bool> shutdown_{false};

public:
    explicit ThreadPool(size_t thread_count);
    ~ThreadPool();

    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator = (const ThreadPool &) = delete;

    bool submit(Task task);
    void shutdown();
};
