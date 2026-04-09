#include "TcpServer.h"
#include "EventLoop.h"
#include <cassert>
#include <stdio.h>

TcpServer::TcpServer(int port, int numIoThreads, int timeoutSec)
    : port_(port)
    , timeoutSec_(timeoutSec)
    , mainLoop_(std::make_unique<EventLoop>())
    , acceptor_(std::make_unique<Acceptor>(mainLoop_.get(), port))
    , numIoThreads_(numIoThreads)
    , nextLoop_(0)
    , nextConnId_(1)
    , connCount_(0)
{
    acceptor_->setNewConnectionCallback(
        [this](int fd, const sockaddr_in& addr){ newConnection(fd, addr); });

    if (timeoutSec_ > 0)
        timerWheel_ = std::make_unique<TimerWheel>(timeoutSec_ * 2);
}

TcpServer::~TcpServer() { stop(); }

void TcpServer::start() {
    // 启动 sub-reactor 线程
    for (int i = 0; i < numIoThreads_; ++i) {
        ioLoops_.push_back(std::make_unique<EventLoop>());
        EventLoop* loop = ioLoops_.back().get();
        ioThreads_.emplace_back([loop]{ loop->loop(); });
    }

    acceptor_->listen();
    printf("[TcpServer] listening on port %d, io-threads=%d\n",
           port_, numIoThreads_);

    mainLoop_->loop();   // 阻塞
}

void TcpServer::stop() {
    mainLoop_->quit();
    for (auto& loop : ioLoops_) loop->quit();
    for (auto& t    : ioThreads_) if (t.joinable()) t.join();
}

EventLoop* TcpServer::nextLoop() {
    if (ioLoops_.empty()) return mainLoop_.get();
    int idx = nextLoop_.fetch_add(1) % static_cast<int>(ioLoops_.size());
    return ioLoops_[idx].get();
}

void TcpServer::newConnection(int sockfd, const sockaddr_in& peerAddr) {
    // 在主线程（main reactor）中调用
    EventLoop* ioLoop = nextLoop();
    int        id     = nextConnId_++;

    auto conn = std::make_shared<TcpConnection>(ioLoop, sockfd, id, peerAddr);

    conn->setConnectionCallback(connCb_);
    conn->setCloseCallback([this](const TcpConnectionPtr& c){
        removeConnection(c);
    });

    if (messageCb_) {
        if (workerPool_) {
            // 用业务线程池处理消息，IO 线程仅负责收发
            conn->setMessageCallback([this](const TcpConnectionPtr& c, Buffer& buf){
                std::string data = buf.retrieveAllAsString();
                workerPool_->submit([this, c, d = std::move(data)]() mutable {
                    Buffer tmp;
                    tmp.append(d);
                    if (messageCb_) messageCb_(c, tmp);
                });
            });
        } else {
            conn->setMessageCallback(messageCb_);
        }
    }

    connections_[id] = conn;
    ++connCount_;

    // 超时踢人
    if (timerWheel_ && timeoutSec_ > 0) {
        timerWheel_->addTimer(timeoutSec_, [conn](){
            if (conn->connected()) {
                printf("[TimerWheel] connection %d timeout, closing\n", conn->id());
                conn->shutdown();
            }
        });
    }

    // 把连接建立的动作投递到对应的 io 线程
    ioLoop->runInLoop([conn]{ conn->connectEstablished(); });

    printf("[TcpServer] new connection [%d] from %s, total=%zu\n",
           id, conn->peerAddrStr().c_str(), connCount_.load());
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn) {
    // 可能在 io 线程中调用，投递到 main loop 统一删除
    mainLoop_->runInLoop([this, conn]{
        connections_.erase(conn->id());
        --connCount_;
        printf("[TcpServer] connection [%d] closed, total=%zu\n",
               conn->id(), connCount_.load());
        EventLoop* ioLoop = nextLoop();
        ioLoop->queueInLoop([conn]{ conn->connectDestroyed(); });
    });
}
