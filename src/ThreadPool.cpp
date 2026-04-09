#include "ThreadPool.h"

ThreadPool::ThreadPool(int numThreads) : stop_(false) {
    for (int i = 0; i < numThreads; ++i) {
        threads_.emplace_back([this]{ workerLoop(); });
    }
}

ThreadPool::~ThreadPool() {
    stop_ = true;
    cv_.notify_all();
    for (auto& t : threads_) {
        if (t.joinable()) t.join();
    }
}

void ThreadPool::submit(Task task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        taskQueue_.push(std::move(task));
    }
    cv_.notify_one();
}

void ThreadPool::workerLoop() {
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]{
                return stop_.load() || !taskQueue_.empty();
            });
            if (stop_ && taskQueue_.empty()) return;
            task = std::move(taskQueue_.front());
            taskQueue_.pop();
        }
        task();
    }
}
