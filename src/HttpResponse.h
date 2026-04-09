#pragma once
#include <string>
#include <unordered_map>
#include "Buffer.h"

class HttpResponse {
public:
    enum StatusCode {
        k200 = 200,
        k301 = 301,
        k400 = 400,
        k403 = 403,
        k404 = 404,
        k500 = 500,
    };

    explicit HttpResponse(bool keepAlive = true)
        : statusCode_(k200), keepAlive_(keepAlive) {}

    void setStatus(StatusCode code)              { statusCode_ = code; }
    void setHeader(const std::string& k,
                   const std::string& v)         { headers_[k] = v; }
    void setBody(const std::string& body)        { body_ = body; }
    void setContentType(const std::string& type) { headers_["Content-Type"] = type; }
    void setKeepAlive(bool ka)                   { keepAlive_ = ka; }

    // 序列化到 Buffer，直接写入发送缓冲区
    void appendToBuffer(Buffer& buf) const;

    // 常用快捷构建
    static HttpResponse ok(const std::string& body,
                           const std::string& contentType = "text/plain; charset=utf-8",
                           bool keepAlive = true);
    static HttpResponse notFound(bool keepAlive = true);
    static HttpResponse serverError(bool keepAlive = true);
    static HttpResponse redirect(const std::string& location, bool keepAlive = false);

private:
    std::string statusMessage() const;

    StatusCode  statusCode_;
    bool        keepAlive_;
    std::string body_;
    std::unordered_map<std::string, std::string> headers_;
};
