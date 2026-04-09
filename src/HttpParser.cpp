#include "HttpParser.h"
#include <cstring>
#include <algorithm>

// 从 Buffer 中取一行（\r\n 结尾），返回 false 表示数据不够
static bool readLine(Buffer& buf, std::string& line) {
    const char* start = buf.peek();
    const char* end   = static_cast<const char*>(
        ::memmem(start, buf.readable(), "\r\n", 2));
    if (!end) return false;
    line.assign(start, end);
    buf.retrieve(end - start + 2);  // 消费包括 \r\n
    return true;
}

HttpParser::Result HttpParser::parse(Buffer& buf) {
    while (true) {
        if (state_ == REQUEST_LINE) {
            std::string line;
            if (!readLine(buf, line)) return AGAIN;
            Result r = parseRequestLine(line);
            if (r != DONE) return r;
            state_ = HEADERS;

        } else if (state_ == HEADERS) {
            std::string line;
            if (!readLine(buf, line)) return AGAIN;

            if (line.empty()) {
                // 空行：Header 结束
                if (contentLength_ > 0) {
                    state_ = BODY;
                } else {
                    state_ = COMPLETE;
                    return DONE;
                }
            } else {
                Result r = parseHeader(line);
                if (r != DONE) return r;
            }

        } else if (state_ == BODY) {
            if (buf.readable() < contentLength_) return AGAIN;
            req_.setBody(std::string(buf.peek(), contentLength_));
            buf.retrieve(contentLength_);
            state_ = COMPLETE;
            return DONE;

        } else {
            return DONE;
        }
    }
}

HttpParser::Result HttpParser::parseRequestLine(const std::string& line) {
    // 格式：METHOD /path?query HTTP/1.1
    size_t p1 = line.find(' ');
    if (p1 == std::string::npos) return ERROR;
    size_t p2 = line.find(' ', p1 + 1);
    if (p2 == std::string::npos) return ERROR;

    req_.setMethod(line.substr(0, p1));

    std::string fullUrl = line.substr(p1 + 1, p2 - p1 - 1);
    size_t qpos = fullUrl.find('?');
    if (qpos != std::string::npos) {
        req_.setUrl(fullUrl.substr(0, qpos));
        req_.setQuery(fullUrl.substr(qpos + 1));
    } else {
        req_.setUrl(fullUrl);
    }

    req_.setVersion(line.substr(p2 + 1));
    return DONE;
}

HttpParser::Result HttpParser::parseHeader(const std::string& line) {
    size_t colon = line.find(':');
    if (colon == std::string::npos) return ERROR;

    std::string key = line.substr(0, colon);
    std::string val = line.substr(colon + 1);

    // 去掉 value 前后空格
    size_t s = val.find_first_not_of(' ');
    if (s != std::string::npos) val = val.substr(s);
    size_t e = val.find_last_not_of(" \r");
    if (e != std::string::npos) val = val.substr(0, e + 1);

    req_.setHeader(key, val);

    if (key == "Content-Length") {
        contentLength_ = static_cast<size_t>(std::stoul(val));
    }
    return DONE;
}
