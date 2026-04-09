# Reactor — C++ 轻量级网络框架

基于 epoll + 线程池的 Reactor 网络框架，在此基础上实现了 HTTP/1.1 服务器。

## 架构

主线程（main reactor）负责监听新连接，accept 后通过 round-robin 分发给子线程。
每个子线程跑一个 EventLoop，由 epoll_wait 驱动 IO 读写。
业务逻辑可选投递到独立线程池，避免阻塞 IO 线程。

## 核心模块

- EventLoop：事件循环，封装 epoll ET 模式，支持跨线程任务投递（eventfd 唤醒）
- Channel：封装 fd 与回调，负责读/写/关闭/错误事件分发
- Acceptor：监听端口，循环 accept 直到 EAGAIN
- TcpConnection：单连接状态机，管理读写缓冲区与生命周期
- Buffer：动态缓冲区，readv 一次读完内核数据，支持自动扩容
- ThreadPool：固定大小线程池，条件变量驱动
- TimerWheel：时间轮定时器，O(1) 踢掉空闲连接
- HttpParser：增量状态机解析 HTTP/1.1 请求（请求行、Header、Body）
- HttpServer：路由注册、静态文件服务、Keep-Alive、per-connection 解析器

## 编译

编译 Echo Server：
g++ -std=c++17 -O2 -pthread \
  src/Buffer.cpp src/Channel.cpp src/EventLoop.cpp \
  src/ThreadPool.cpp src/TimerWheel.cpp src/Acceptor.cpp \
  src/TcpConnection.cpp src/TcpServer.cpp \
  example/echo_server.cpp -Isrc -o echo_server

编译 HTTP Server：
g++ -std=c++17 -O2 -pthread \
  src/Buffer.cpp src/Channel.cpp src/EventLoop.cpp \
  src/ThreadPool.cpp src/TimerWheel.cpp src/Acceptor.cpp \
  src/TcpConnection.cpp src/TcpServer.cpp \
  src/HttpRequest.cpp src/HttpResponse.cpp \
  src/HttpParser.cpp src/HttpServer.cpp \
  example/http_server.cpp -Isrc -o http_server

## 运行

Echo Server：
  启动：./echo_server 9000 4
  测试：echo "hello" | nc 127.0.0.1 9000
  压测：200 并发实测 OK=200 FAIL=0

HTTP Server：
  启动：./http_server 8080 4
  GET  /        首页
  GET  /ping    健康检查（返回 pong）
  GET  /time    服务器时间（返回 JSON）
  POST /echo    回显请求 body
  GET  /static/ 静态文件服务（映射到 ./static/ 目录）
  压测：100 并发实测 OK=100/100

## 自定义业务（Reactor）

TcpServer server(9000, 4, 60);
server.setMessageCallback([](const TcpConnectionPtr& conn, Buffer& buf) {
    conn->send(buf.retrieveAllAsString());
});
server.start();

## 自定义路由（HTTP）

HttpServer server(8080, 4, 60);
server.GET("/hello", [](const HttpRequest& req) {
    return HttpResponse::ok("hello world\n");
});
server.POST("/data", [](const HttpRequest& req) {
    return HttpResponse::ok(req.body(), "application/json");
});
server.serveStatic("/static", "./static");
server.start();
