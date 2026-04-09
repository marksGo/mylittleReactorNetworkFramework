#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

// 固定大小线程池，用于处理业务逻辑（IO 线程只负责收发）
class ThreadPool {
public:
    using Task = std::function<void()>;

    explicit ThreadPool(int numThreads = 4);
    ~ThreadPool();

    // 提交任务（线程安全）
    void submit(Task task);

    int size() const { return static_cast<int>(threads_.size()); }

private:
    void workerLoop();

    std::vector<std::thread>  threads_;
    std::queue<Task>          taskQueue_;
    std::mutex                mutex_;
    std::condition_variable   cv_;
    std::atomic<bool>         stop_;
};
