#pragma once
#include "Buffer.h"
#include "Channel.h"
#include <functional>
#include <memory>
#include <string>
#include <netinet/in.h>

class EventLoop;
class TcpConnection;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using MessageCallback  = std::function<void(const TcpConnectionPtr&, Buffer&)>;
using CloseCallback    = std::function<void(const TcpConnectionPtr&)>;
using ConnectionCallback = std::function<void(const TcpConnectionPtr&, bool /*connected*/)>;

// TcpConnection：代表一条已建立的 TCP 连接
// 生命周期由 shared_ptr 管理
class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    TcpConnection(EventLoop* loop, int sockfd, int id, const sockaddr_in& peerAddr);
    ~TcpConnection();

    int  id()   const { return id_; }
    bool connected() const { return state_ == kConnected; }

    // 线程安全发送（可从任意线程调用）
    void send(const std::string& data);
    void send(const char* data, size_t len);

    // 主动关闭写端
    void shutdown();

    // 回调设置（由 TcpServer 设置）
    void setMessageCallback   (MessageCallback   cb) { messageCb_    = std::move(cb); }
    void setCloseCallback     (CloseCallback     cb) { closeCb_      = std::move(cb); }
    void setConnectionCallback(ConnectionCallback cb){ connectionCb_ = std::move(cb); }

    // 连接建立后由 TcpServer 调用一次
    void connectEstablished();
    // 连接销毁前由 TcpServer 调用一次
    void connectDestroyed();

    std::string peerAddrStr() const;

private:
    enum State { kConnecting, kConnected, kDisconnecting, kDisconnected };

    void handleRead();
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const char* data, size_t len);
    void shutdownInLoop();

    EventLoop*  loop_;
    const int   id_;
    int         sockfd_;
    State       state_;
    sockaddr_in peerAddr_;

    Channel     channel_;
    Buffer      readBuf_;
    Buffer      writeBuf_;

    MessageCallback    messageCb_;
    CloseCallback      closeCb_;
    ConnectionCallback connectionCb_;
};
