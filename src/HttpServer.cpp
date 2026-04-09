#include "HttpServer.h"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <stdio.h>

// 简单 MIME 类型映射
static std::string mimeType(const std::string& path) {
    static const std::unordered_map<std::string, std::string> types = {
        {".html", "text/html; charset=utf-8"},
        {".htm",  "text/html; charset=utf-8"},
        {".css",  "text/css"},
        {".js",   "application/javascript"},
        {".json", "application/json"},
        {".png",  "image/png"},
        {".jpg",  "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif",  "image/gif"},
        {".svg",  "image/svg+xml"},
        {".ico",  "image/x-icon"},
        {".txt",  "text/plain; charset=utf-8"},
    };
    auto dot = path.rfind('.');
    if (dot != std::string::npos) {
        auto it = types.find(path.substr(dot));
        if (it != types.end()) return it->second;
    }
    return "application/octet-stream";
}

HttpServer::HttpServer(int port, int numIoThreads, int timeoutSec)
    : server_(port, numIoThreads, timeoutSec)
{
    notFoundHandler_ = [](const HttpRequest&) {
        return HttpResponse::notFound();
    };
}


void HttpServer::GET(const std::string& path, HttpHandler handler) {
    routes_["GET"][path] = std::move(handler);
}

void HttpServer::POST(const std::string& path, HttpHandler handler) {
    routes_["POST"][path] = std::move(handler);
}

void HttpServer::serveStatic(const std::string& urlPrefix,
                              const std::string& rootDir) {
    staticUrlPrefix_ = urlPrefix;
    staticRootDir_   = rootDir;
}

void HttpServer::start() {
    // 重新设置回调（覆盖构造函数里的临时版本）
    // 用 per-connection parser map
    auto parsers = std::make_shared<
        std::unordered_map<int, std::shared_ptr<HttpParser>>>();
    auto mu = std::make_shared<std::mutex>();

    server_.setConnectionCallback([parsers, mu]
        (const TcpConnectionPtr& conn, bool connected) {
        std::lock_guard<std::mutex> lock(*mu);
        if (connected)  (*parsers)[conn->id()] = std::make_shared<HttpParser>();
        else             parsers->erase(conn->id());
    });

    server_.setMessageCallback([this, parsers, mu]
        (const TcpConnectionPtr& conn, Buffer& buf) {
        std::shared_ptr<HttpParser> parser;
        {
            std::lock_guard<std::mutex> lock(*mu);
            auto it = parsers->find(conn->id());
            if (it == parsers->end()) return;
            parser = it->second;
        }

        while (buf.readable() > 0) {
            auto result = parser->parse(buf);
            if (result == HttpParser::DONE) {
                handleRequest(conn, parser->request());
                bool ka = parser->request().keepAlive();
                parser->reset();
                if (!ka) { conn->shutdown(); break; }
            } else if (result == HttpParser::ERROR) {
                // 400 Bad Request
                HttpResponse resp(false);
                resp.setStatus(HttpResponse::k400);
                resp.setContentType("text/plain");
                resp.setBody("400 Bad Request");
                Buffer out;
                resp.appendToBuffer(out);
                conn->send(out.retrieveAllAsString());
                conn->shutdown();
                break;
            } else {
                break;  // AGAIN：等待更多数据
            }
        }
    });

    printf("[HttpServer] starting on port...\n");
    server_.start();
}

void HttpServer::handleRequest(const TcpConnectionPtr& conn,
                                const HttpRequest& req) {
    printf("[HTTP] %s %s\n", req.methodStr().c_str(), req.url().c_str());

    HttpResponse resp = dispatchRoute(req);
    resp.setKeepAlive(req.keepAlive());

    Buffer out;
    resp.appendToBuffer(out);
    conn->send(out.retrieveAllAsString());
}

HttpResponse HttpServer::dispatchRoute(const HttpRequest& req) {
    // 1. 静态文件
    if (!staticUrlPrefix_.empty() &&
        req.url().substr(0, staticUrlPrefix_.size()) == staticUrlPrefix_) {
        std::string relPath = req.url().substr(staticUrlPrefix_.size());
        if (relPath.empty()) relPath = "/index.html";
        std::string filePath = staticRootDir_ + relPath;
        return serveFile(filePath);
    }

    // 2. 路由表
    auto mit = routes_.find(req.methodStr());
    if (mit != routes_.end()) {
        auto pit = mit->second.find(req.url());
        if (pit != mit->second.end()) {
            try {
                return pit->second(req);
            } catch (const std::exception& e) {
                fprintf(stderr, "handler exception: %s\n", e.what());
                return HttpResponse::serverError();
            }
        }
    }

    // 3. 404
    return notFoundHandler_(req);
}

HttpResponse HttpServer::serveFile(const std::string& filePath) {
    // 简单安全检查：防止路径穿越
    if (filePath.find("..") != std::string::npos)
        return HttpResponse::notFound();

    std::ifstream f(filePath, std::ios::binary);
    if (!f) return HttpResponse::notFound();

    std::ostringstream ss;
    ss << f.rdbuf();
    return HttpResponse::ok(ss.str(), mimeType(filePath));
}
