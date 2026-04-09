#include "../src/HttpServer.h"
#include <signal.h>
#include <sstream>
#include <ctime>

// ── HTTP Server 示例 ──────────────────────────────────────────────
//
// 路由：
//   GET  /          → 首页 HTML
//   GET  /ping      → pong（健康检查）
//   GET  /time      → 当前服务器时间（JSON）
//   POST /echo      → 回显请求 body
//   GET  /static/*  → 静态文件服务（./static/ 目录）
//
// 启动：./http_server [port=8080] [io_threads=4]
// 测试：curl http://127.0.0.1:8080/ping
//       curl http://127.0.0.1:8080/time
//       curl -X POST -d "hello" http://127.0.0.1:8080/echo

static HttpServer* g_server = nullptr;

int main(int argc, char* argv[]) {
    int port      = argc > 1 ? atoi(argv[1]) : 8080;
    int ioThreads = argc > 2 ? atoi(argv[2]) : 4;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT,  [](int){ if (g_server) exit(0); });

    HttpServer server(port, ioThreads, /*timeoutSec=*/60);
    g_server = &server;

    // ── GET / ────────────────────────────────────────────────────
    server.GET("/", [](const HttpRequest&) {
        std::string html = R"(<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>Reactor HTTP</title></head>
<body>
<h1>🚀 Reactor HTTP Server</h1>
<ul>
  <li><a href="/ping">GET /ping</a> — 健康检查</li>
  <li><a href="/time">GET /time</a> — 服务器时间</li>
  <li>POST /echo — 回显请求 body</li>
</ul>
<p>基于 epoll + Reactor 的 C++ HTTP 服务器</p>
</body></html>)";
        return HttpResponse::ok(html, "text/html; charset=utf-8");
    });

    // ── GET /ping ────────────────────────────────────────────────
    server.GET("/ping", [](const HttpRequest&) {
        return HttpResponse::ok("pong\n");
    });

    // ── GET /time ────────────────────────────────────────────────
    server.GET("/time", [](const HttpRequest&) {
        std::time_t t = std::time(nullptr);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        std::string json = std::string("{\"time\":\"") + buf + "\"}";
        return HttpResponse::ok(json, "application/json");
    });

    // ── POST /echo ───────────────────────────────────────────────
    server.POST("/echo", [](const HttpRequest& req) {
        std::string body = req.body();
        if (body.empty()) body = "(empty body)";
        return HttpResponse::ok(body, "text/plain; charset=utf-8");
    });

    // ── 静态文件 /static/* → ./static/ ──────────────────────────
    server.serveStatic("/static", "./static");

    printf("[HttpServer] listening on http://127.0.0.1:%d\n", port);
    printf("  GET  /        — 首页\n");
    printf("  GET  /ping    — 健康检查\n");
    printf("  GET  /time    — 服务器时间\n");
    printf("  POST /echo    — 回显 body\n");
    printf("  GET  /static/ — 静态文件\n\n");

    server.start();
    return 0;
}
