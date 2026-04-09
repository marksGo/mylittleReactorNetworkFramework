#pragma once
#include "TcpConnection.h"
#include "Acceptor.h"
#include "ThreadPool.h"
#include "TimerWheel.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>

class EventLoop;

// TcpServer：对外暴露的最高层接口
// 采用 one loop per thread：主线程 accept，N 个 sub-reactor 线程处理 IO
class TcpServer {
public:
    // port        : 监听端口
    // numIoThreads: sub-reactor 数量（0 = 单线程模式，accept 与 IO 同线程）
    // timeoutSec  : 连接空闲超时秒数（0 = 不超时）
    TcpServer(int port,
              int numIoThreads = std::thread::hardware_concurrency(),
              int timeoutSec   = 60);
    ~TcpServer();

    // 设置业务回调（必须在 start() 前调用）
    void setConnectionCallback(ConnectionCallback cb) { connCb_    = std::move(cb); }
    void setMessageCallback   (MessageCallback    cb) { messageCb_ = std::move(cb); }

    // 使用业务线程池处理消息（不设则在 IO 线程中直接调用 messageCb）
    void setWorkerThreads(int n) { workerPool_ = std::make_unique<ThreadPool>(n); }

    void start();   // 启动（阻塞，在调用线程跑主 reactor）
    void stop();

    size_t connectionCount() const { return connCount_.load(); }

private:
    void newConnection(int sockfd, const sockaddr_in& peerAddr);
    void removeConnection(const TcpConnectionPtr& conn);
    EventLoop* nextLoop();   // round-robin 选一个 sub-reactor

    int  port_;
    int  timeoutSec_;

    std::unique_ptr<EventLoop>   mainLoop_;
    std::unique_ptr<Acceptor>    acceptor_;

    // Sub-reactors（每个跑在独立线程）
    int                              numIoThreads_;
    std::vector<std::unique_ptr<EventLoop>>  ioLoops_;
    std::vector<std::thread>                 ioThreads_;
    std::atomic<int>                         nextLoop_;

    std::unordered_map<int, TcpConnectionPtr>  connections_;
    std::atomic<int>   nextConnId_;
    std::atomic<size_t> connCount_;

    ConnectionCallback connCb_;
    MessageCallback    messageCb_;

    std::unique_ptr<ThreadPool>  workerPool_;
    std::unique_ptr<TimerWheel>  timerWheel_;
};
