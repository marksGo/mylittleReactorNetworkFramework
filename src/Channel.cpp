#include "Channel.h"
#include "EventLoop.h"
#include <sys/epoll.h>
#include <stdio.h>

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), addedToLoop_(false) {}

Channel::~Channel() {}

void Channel::update() {
    addedToLoop_ = true;
    loop_->updateChannel(this);
}

void Channel::remove() {
    loop_->removeChannel(this);
}

void Channel::handleEvent() {
    // 对端关闭连接
    if ((revents_ & EPOLLRDHUP) && !(revents_ & EPOLLIN)) {
        if (closeCb_) closeCb_();
        return;
    }
    // 错误
    if (revents_ & (EPOLLERR | EPOLLHUP)) {
        if (errorCb_) errorCb_();
        return;
    }
    // 可读
    if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
        if (readCb_) readCb_();
    }
    // 可写
    if (revents_ & EPOLLOUT) {
        if (writeCb_) writeCb_();
    }
}
