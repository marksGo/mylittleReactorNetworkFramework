#pragma once
#include "Channel.h"
#include <functional>
#include <netinet/in.h>

class EventLoop;

// Acceptor：运行在主线程（main reactor），负责 accept 新连接
// accept 到 fd 后通过回调交给 TcpServer 分配到 sub reactor
class Acceptor {
public:
    using NewConnectionCb = std::function<void(int sockfd, const sockaddr_in&)>;

    Acceptor(EventLoop* loop, int port, bool reusePort = true);
    ~Acceptor();

    void setNewConnectionCallback(NewConnectionCb cb) { newConnCb_ = std::move(cb); }

    void listen();
    bool listening() const { return listening_; }

private:
    void handleRead();   // epoll 通知有新连接

    int      listenFd_;
    bool     listening_;
    Channel  acceptChannel_;
    NewConnectionCb newConnCb_;
};
