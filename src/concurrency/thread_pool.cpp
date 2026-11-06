#include "concurrency/thread_pool.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <mutex>
#include <utility>

bool TaskQueue::push(Task task){
    {
       std::lock_guard<std::mutex> lock(mutex_);
       
       if(stopping_)
          return false;

       tasks_.push_back(std::move(task));
    }

    cv_.notify_one();

    return true;
}

bool TaskQueue::pop(Task &task){
    std::unique_lock<std::mutex> lock(mutex_);
    
    cv_.wait(lock, [this]{
        return stopping_ || !tasks_.empty();
    });

    if(tasks_.empty())
        return false;

    task = std::move(tasks_.front());
    tasks_.pop_front();

    return true;
}

void TaskQueue::stop(){
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }

    cv_.notify_all();
}

ThreadPool::ThreadPool(size_t thread_count){
    thread_count = std::max<size_t>(thread_count, 1);
    workers_.reserve(thread_count);

    try{
        for(size_t i = 0; i < thread_count; i++)
            workers_.emplace_back(&ThreadPool::worker_loop, this);
    }
    catch(...){
        queue_.stop();

        for(auto &worker : workers_)
            if(worker.joinable())
                worker.join();

        throw;
    }
}

ThreadPool::~ThreadPool(){
    shutdown();
}

bool ThreadPool::submit(Task task){
    return queue_.push(std::move(task));
}

void ThreadPool::shutdown(){
    if(shutdown_.exchange(true))
        return;

    queue_.stop();

    for(auto &worker : workers_)
        if(worker.joinable())
            worker.join();

    workers_.clear();
}

void ThreadPool::worker_loop(){
    Task task;
    
    while(queue_.pop(task)){
        try{
            task();
        }
        catch(const std::exception &error){
            std::fprintf(stderr, "task failed: %s\n", error.what());
        }
        catch(...) {
            std::fprintf(stderr, "task failed: Unknown exception.\n");
        }
    }
}
