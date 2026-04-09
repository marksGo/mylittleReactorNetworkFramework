#pragma once
#include <functional>
#include <vector>
#include <mutex>
#include <atomic>
#include <sys/epoll.h>

class Channel;

// EventLoop：一个线程一个 loop（one loop per thread）
// 封装 epoll_wait，负责事件分发与跨线程任务调度
class EventLoop {
public:
    using Task = std::function<void()>;

    EventLoop();
    ~EventLoop();

    // 启动事件循环（阻塞）
    void loop();
    // 退出事件循环
    void quit();

    // 在本 loop 线程执行任务；如果调用者就是本线程，直接执行，否则放入队列
    void runInLoop(Task task);
    // 无论如何都放入队列，唤醒后执行
    void queueInLoop(Task task);

    // 由 Channel 调用，向 epoll 注册/修改/删除 fd
    void updateChannel(Channel* ch);
    void removeChannel(Channel* ch);

    bool isInLoopThread() const;

private:
    void wakeup();          // 写 eventfd，唤醒 epoll_wait
    void handleWakeup();    // 读 eventfd
    void doPendingTasks();  // 执行所有排队任务

    static const int kMaxEvents = 1024;
    static const int kEpollTimeout = 5000;  // ms

    int epollFd_;
    int wakeupFd_;          // eventfd，用于跨线程唤醒

    std::atomic<bool> quit_;
    std::atomic<pid_t>        threadId_;  // 所属线程 id，loop() 调用时设置

    std::vector<epoll_event>  activeEvents_;
    std::vector<Channel*>     channels_;      // 所有注册的 channel（for debug）

    std::mutex          mutex_;
    std::vector<Task>   pendingTasks_;
    bool                callingPendingTasks_;
};
