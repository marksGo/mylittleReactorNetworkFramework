#include "TcpConnection.h"
#include "EventLoop.h"
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <cassert>

TcpConnection::TcpConnection(EventLoop* loop, int sockfd, int id,
                             const sockaddr_in& peerAddr)
    : loop_(loop), id_(id), sockfd_(sockfd)
    , state_(kConnecting), peerAddr_(peerAddr)
    , channel_(loop, sockfd)
{
    // 开启 TCP keepalive
    int on = 1;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
    // 禁用 Nagle 算法（游戏/实时场景减少延迟）
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));

    channel_.setReadCallback ([this]{ handleRead();  });
    channel_.setWriteCallback([this]{ handleWrite(); });
    channel_.setCloseCallback([this]{ handleClose(); });
    channel_.setErrorCallback([this]{ handleError(); });
}

TcpConnection::~TcpConnection() {
    ::close(sockfd_);
}

std::string TcpConnection::peerAddrStr() const {
    char buf[64];
    ::inet_ntop(AF_INET, &peerAddr_.sin_addr, buf, sizeof(buf));
    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
             ":%d", ntohs(peerAddr_.sin_port));
    return buf;
}

void TcpConnection::connectEstablished() {
    assert(state_ == kConnecting);
    state_ = kConnected;
    channel_.enableReading();
    if (connectionCb_) connectionCb_(shared_from_this(), true);
}

void TcpConnection::connectDestroyed() {
    if (state_ == kConnected) {
        state_ = kDisconnected;
        channel_.disableAll();
    }
    channel_.remove();
    if (connectionCb_) connectionCb_(shared_from_this(), false);
}

// ── 发送 ─────────────────────────────────────────────────────────
void TcpConnection::send(const std::string& data) {
    send(data.data(), data.size());
}

void TcpConnection::send(const char* data, size_t len) {
    if (state_ != kConnected) return;
    if (loop_->isInLoopThread()) {
        sendInLoop(data, len);
    } else {
        // 跨线程：复制数据后投递到 loop 线程
        std::string copy(data, len);
        loop_->runInLoop([this, s = std::move(copy)]{
            sendInLoop(s.data(), s.size());
        });
    }
}

void TcpConnection::sendInLoop(const char* data, size_t len) {
    if (state_ == kDisconnected) return;

    ssize_t nwritten = 0;
    size_t  remaining = len;

    // 如果写缓冲区为空，直接尝试写（减少一次 epoll 注册）
    if (!channel_.isWriting() && writeBuf_.readable() == 0) {
        nwritten = ::write(sockfd_, data, len);
        if (nwritten < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("write");
                return;
            }
            nwritten = 0;
        } else {
            remaining = len - static_cast<size_t>(nwritten);
        }
    }

    // 写不完的数据放入 writeBuf_，注册 EPOLLOUT 等内核通知
    if (remaining > 0) {
        writeBuf_.append(data + nwritten, remaining);
        if (!channel_.isWriting()) channel_.enableWriting();
    }
}

void TcpConnection::shutdown() {
    if (state_ == kConnected) {
        state_ = kDisconnecting;
        loop_->runInLoop([this]{ shutdownInLoop(); });
    }
}

void TcpConnection::shutdownInLoop() {
    if (!channel_.isWriting())
        ::shutdown(sockfd_, SHUT_WR);
}

// ── 事件处理 ──────────────────────────────────────────────────────
void TcpConnection::handleRead() {
    int err = 0;
    ssize_t n = readBuf_.readFd(sockfd_, &err);
    if (n > 0) {
        if (messageCb_) messageCb_(shared_from_this(), readBuf_);
    } else if (n == 0) {
        handleClose();
    } else {
        errno = err;
        if (errno != EAGAIN) handleError();
    }
}

void TcpConnection::handleWrite() {
    if (!channel_.isWriting()) return;
    ssize_t n = ::write(sockfd_, writeBuf_.peek(), writeBuf_.readable());
    if (n > 0) {
        writeBuf_.retrieve(static_cast<size_t>(n));
        if (writeBuf_.readable() == 0) {
            channel_.disableWriting();
            if (state_ == kDisconnecting) shutdownInLoop();
        }
    } else {
        perror("handleWrite");
    }
}

void TcpConnection::handleClose() {
    state_ = kDisconnected;
    channel_.disableAll();
    TcpConnectionPtr guardThis(shared_from_this());
    if (connectionCb_) connectionCb_(guardThis, false);
    if (closeCb_)      closeCb_(guardThis);
}

void TcpConnection::handleError() {
    int err = 0;
    socklen_t len = sizeof(err);
    ::getsockopt(sockfd_, SOL_SOCKET, SO_ERROR, &err, &len);
    if (err != 0)
        fprintf(stderr, "TcpConnection[%d] error: %s\n", id_, strerror(err));
    handleClose();
}
