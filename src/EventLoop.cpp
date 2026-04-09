#include "EventLoop.h"
#include "Channel.h"
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <stdexcept>
#include <cassert>
#include <cstring>
#include <stdio.h>
#include <sys/syscall.h>

static pid_t myGetTid() { return static_cast<pid_t>(::syscall(SYS_gettid)); }

EventLoop::EventLoop()
    : epollFd_(::epoll_create1(EPOLL_CLOEXEC))
    , wakeupFd_(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC))
    , quit_(false)
    , threadId_(0)             // 在 loop() 中才确定所属线程
    , activeEvents_(kMaxEvents)
    , callingPendingTasks_(false)
{
    if (epollFd_ < 0 || wakeupFd_ < 0)
        throw std::runtime_error("EventLoop: epoll_create1/eventfd failed");

    // 监听 wakeupFd_ 可读事件
    epoll_event ev{};
    ev.events  = EPOLLIN;
    ev.data.ptr = nullptr;   // 用 nullptr 区分 wakeupFd_
    // 我们用特殊标记：把 wakeupFd_ 包一个专属 channel
    // 简单起见这里直接加入 epoll
    struct WakeChannel {
        static void* make(int fd) {
            // 返回一个哑指针区分 wakeup fd
            return reinterpret_cast<void*>(static_cast<intptr_t>(fd) | 1LL<<32);
        }
    };
    ev.data.fd = wakeupFd_;
    ::epoll_ctl(epollFd_, EPOLL_CTL_ADD, wakeupFd_, &ev);
}

EventLoop::~EventLoop() {
    ::close(wakeupFd_);
    ::close(epollFd_);
}

bool EventLoop::isInLoopThread() const {
    return threadId_ == myGetTid();
}

void EventLoop::loop() {
    threadId_ = myGetTid();   // 在实际运行的线程中记录
    quit_ = false;

    while (!quit_) {
        int n = ::epoll_wait(epollFd_, activeEvents_.data(),
                             static_cast<int>(activeEvents_.size()),
                             kEpollTimeout);
        if (n < 0 && errno != EINTR) {
            perror("epoll_wait");
            break;
        }
        for (int i = 0; i < n; ++i) {
            auto& ev = activeEvents_[i];
            if (ev.data.fd == wakeupFd_) {
                handleWakeup();
                continue;
            }
            Channel* ch = static_cast<Channel*>(ev.data.ptr);
            ch->setRevents(static_cast<int>(ev.events));
            ch->handleEvent();
        }
        doPendingTasks();
    }
}

void EventLoop::quit() {
    quit_ = true;
    if (!isInLoopThread()) wakeup();
}

void EventLoop::runInLoop(Task task) {
    if (isInLoopThread()) task();
    else                   queueInLoop(std::move(task));
}

void EventLoop::queueInLoop(Task task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingTasks_.push_back(std::move(task));
    }
    // 如果不在本线程，或正在执行 pending tasks（可能又 queue 了新任务），都需要唤醒
    if (!isInLoopThread() || callingPendingTasks_) wakeup();
}

void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = ::write(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one)) perror("wakeup write");
}

void EventLoop::handleWakeup() {
    uint64_t one;
    ssize_t n = ::read(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one)) perror("wakeup read");
}

void EventLoop::doPendingTasks() {
    std::vector<Task> tasks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks.swap(pendingTasks_);
    }
    callingPendingTasks_ = true;
    for (auto& t : tasks) t();
    callingPendingTasks_ = false;
}

void EventLoop::updateChannel(Channel* ch) {
    epoll_event ev{};
    ev.events   = static_cast<uint32_t>(ch->events());
    ev.data.ptr = ch;
    if (::epoll_ctl(epollFd_, EPOLL_CTL_ADD, ch->fd(), &ev) < 0) {
        if (errno == EEXIST)
            ::epoll_ctl(epollFd_, EPOLL_CTL_MOD, ch->fd(), &ev);
    }
}

void EventLoop::removeChannel(Channel* ch) {
    epoll_event ev{};
    ::epoll_ctl(epollFd_, EPOLL_CTL_DEL, ch->fd(), &ev);
}
