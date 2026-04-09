#include "HttpResponse.h"
#include <sstream>

std::string HttpResponse::statusMessage() const {
    switch (statusCode_) {
        case k200: return "OK";
        case k301: return "Moved Permanently";
        case k400: return "Bad Request";
        case k403: return "Forbidden";
        case k404: return "Not Found";
        case k500: return "Internal Server Error";
        default:   return "Unknown";
    }
}

void HttpResponse::appendToBuffer(Buffer& buf) const {
    // 状态行
    std::string line = "HTTP/1.1 " + std::to_string(static_cast<int>(statusCode_))
                     + " " + statusMessage() + "\r\n";
    buf.append(line);

    // Connection header
    buf.append(keepAlive_ ? "Connection: keep-alive\r\n"
                           : "Connection: close\r\n");

    // Content-Length
    buf.append("Content-Length: " + std::to_string(body_.size()) + "\r\n");

    // 其他 headers
    for (auto& [k, v] : headers_) {
        buf.append(k + ": " + v + "\r\n");
    }

    // 空行 + body
    buf.append("\r\n");
    buf.append(body_);
}

HttpResponse HttpResponse::ok(const std::string& body,
                               const std::string& contentType,
                               bool keepAlive) {
    HttpResponse resp(keepAlive);
    resp.setStatus(k200);
    resp.setContentType(contentType);
    resp.setBody(body);
    return resp;
}

HttpResponse HttpResponse::notFound(bool keepAlive) {
    HttpResponse resp(keepAlive);
    resp.setStatus(k404);
    resp.setContentType("text/plain; charset=utf-8");
    resp.setBody("404 Not Found");
    return resp;
}

HttpResponse HttpResponse::serverError(bool keepAlive) {
    HttpResponse resp(keepAlive);
    resp.setStatus(k500);
    resp.setContentType("text/plain; charset=utf-8");
    resp.setBody("500 Internal Server Error");
    return resp;
}

HttpResponse HttpResponse::redirect(const std::string& location, bool keepAlive) {
    HttpResponse resp(keepAlive);
    resp.setStatus(k301);
    resp.setHeader("Location", location);
    resp.setBody("");
    return resp;
}
