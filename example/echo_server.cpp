#include "../src/TcpServer.h"
#include <signal.h>
#include <stdio.h>

// ── Echo Server：收到什么就回什么 ──────────────────────────────────
//
// 启动：./echo_server [port=9000] [io_threads=4] [worker_threads=0]
// 测试：echo "hello reactor" | nc 127.0.0.1 9000
//       或用 ab/wrk 压测：
//       for i in $(seq 1 1000); do echo "ping" | nc -q1 127.0.0.1 9000 & done

static TcpServer* g_server = nullptr;

int main(int argc, char* argv[]) {
    int port           = argc > 1 ? atoi(argv[1]) : 9000;
    int ioThreads      = argc > 2 ? atoi(argv[2]) : 4;
    int workerThreads  = argc > 3 ? atoi(argv[3]) : 0;

    // 忽略 SIGPIPE（对端关闭时不要让进程退出）
    signal(SIGPIPE, SIG_IGN);

    TcpServer server(port, ioThreads, /*timeoutSec=*/60);

    // 连接建立/断开回调
    server.setConnectionCallback([](const TcpConnectionPtr& conn, bool connected){
        printf("[App] conn[%d] %s %s\n",
               conn->id(),
               conn->peerAddrStr().c_str(),
               connected ? "connected" : "disconnected");
    });

    // 消息回调：直接回显
    server.setMessageCallback([](const TcpConnectionPtr& conn, Buffer& buf){
        std::string msg = buf.retrieveAllAsString();
        printf("[App] conn[%d] recv %zu bytes: %s",
               conn->id(), msg.size(), msg.c_str());
        conn->send(msg);   // echo back
    });

    if (workerThreads > 0) {
        server.setWorkerThreads(workerThreads);
        printf("[App] worker threads: %d\n", workerThreads);
    }

    g_server = &server;
    signal(SIGINT, [](int){ if (g_server) g_server->stop(); });

    server.start();   // 阻塞直到 stop()
    return 0;
}
