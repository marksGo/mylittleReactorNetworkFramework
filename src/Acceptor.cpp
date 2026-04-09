#include "Acceptor.h"
#include "EventLoop.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include <stdio.h>

static int createNonBlockSocket() {
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) throw std::runtime_error("socket() failed");
    return fd;
}

Acceptor::Acceptor(EventLoop* loop, int port, bool reusePort)
    : listenFd_(createNonBlockSocket())
    , listening_(false)
    , acceptChannel_(loop, listenFd_)
{
    int on = 1;
    ::setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (reusePort)
        ::setsockopt(listenFd_, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(static_cast<uint16_t>(port));

    if (::bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error("bind() failed");

    acceptChannel_.setReadCallback([this]{ handleRead(); });
}

Acceptor::~Acceptor() {
    acceptChannel_.disableAll();
    acceptChannel_.remove();
    ::close(listenFd_);
}

void Acceptor::listen() {
    listening_ = true;
    ::listen(listenFd_, SOMAXCONN);
    acceptChannel_.enableReading();
}

void Acceptor::handleRead() {
    sockaddr_in peerAddr{};
    socklen_t   addrLen = sizeof(peerAddr);

    // ET 模式下循环 accept，直到 EAGAIN
    while (true) {
        int connfd = ::accept4(listenFd_,
                               reinterpret_cast<sockaddr*>(&peerAddr),
                               &addrLen,
                               SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (connfd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            perror("accept4");
            break;
        }
        if (newConnCb_) newConnCb_(connfd, peerAddr);
        else            ::close(connfd);
    }
}
