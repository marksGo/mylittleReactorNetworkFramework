#pragma once
#include <functional>
#include <sys/epoll.h>

class EventLoop;

// Channel 封装一个 fd 及其关注的事件和回调
// 不拥有 fd 的生命周期
class Channel {
public:
    using EventCallback = std::function<void()>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    // 设置回调
    void setReadCallback (EventCallback cb) { readCb_  = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCb_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCb_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCb_ = std::move(cb); }

    int fd()     const { return fd_; }
    int events() const { return events_; }

    // 由 EventLoop 调用，告知实际发生的事件
    void setRevents(int revt) { revents_ = revt; }

    // 事件开关
    void enableReading()  { events_ |=  (EPOLLIN | EPOLLRDHUP); update(); }
    void disableReading() { events_ &= ~(EPOLLIN | EPOLLRDHUP); update(); }
    void enableWriting()  { events_ |=  EPOLLOUT; update(); }
    void disableWriting() { events_ &= ~EPOLLOUT; update(); }
    void disableAll()     { events_  =  0;         update(); }

    bool isWriting() const { return events_ & EPOLLOUT; }
    bool isReading() const { return events_ & EPOLLIN;  }
    bool isNoneEvent() const { return events_ == 0; }

    // 从 epoll 移除并禁用所有事件
    void remove();

    // 分发事件，由 EventLoop 在 poll 返回后调用
    void handleEvent();

private:
    void update();   // 向 EventLoop/epoll 注册事件变更

    EventLoop* loop_;
    const int  fd_;
    int        events_;   // 关注的事件
    int        revents_;  // epoll 返回的实际事件
    bool       addedToLoop_;

    EventCallback readCb_;
    EventCallback writeCb_;
    EventCallback closeCb_;
    EventCallback errorCb_;
};
