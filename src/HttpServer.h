#pragma once
#include "TcpServer.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "HttpParser.h"
#include <functional>
#include <unordered_map>
#include <string>
#include <filesystem>

// 路由 handler 类型
using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

// HttpServer：在 TcpServer 上封装 HTTP 协议层
// 支持路由注册、静态文件服务、Keep-Alive
class HttpServer {
public:
    HttpServer(int port,
               int numIoThreads = 4,
               int timeoutSec   = 60);

    // 注册路由：GET /path → handler
    void GET (const std::string& path, HttpHandler handler);
    void POST(const std::string& path, HttpHandler handler);

    // 静态文件服务：访问 /static/xxx 映射到 rootDir/xxx
    void serveStatic(const std::string& urlPrefix,
                     const std::string& rootDir);

    // 全局 404 handler（可覆盖默认行为）
    void setNotFoundHandler(HttpHandler h) { notFoundHandler_ = std::move(h); }

    void start();   // 阻塞

private:
    void onMessage(const TcpConnectionPtr& conn, Buffer& buf);
    void handleRequest(const TcpConnectionPtr& conn, const HttpRequest& req);
    HttpResponse dispatchRoute(const HttpRequest& req);
    HttpResponse serveFile(const std::string& filePath);

    // 每个连接独立的解析器状态，存在连接的 context 中
    struct ConnContext {
        HttpParser parser;
    };

    TcpServer   server_;
    // method -> path -> handler
    std::unordered_map<std::string,
        std::unordered_map<std::string, HttpHandler>> routes_;

    std::string staticUrlPrefix_;
    std::string staticRootDir_;

    HttpHandler notFoundHandler_;
};
