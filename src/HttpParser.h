#pragma once
#include "HttpRequest.h"
#include "Buffer.h"
#include <string>

// 增量式 HTTP 解析器
// 每次调用 parse() 消费 Buffer 中已到达的数据
// 返回 DONE 表示一个完整请求已解析完毕
class HttpParser {
public:
    enum Result { DONE, AGAIN, ERROR };

    HttpParser() : state_(REQUEST_LINE), contentLength_(0) {}

    // 解析 buf 中的数据，成功时 request() 返回解析结果
    Result parse(Buffer& buf);

    const HttpRequest& request() const { return req_; }
    void reset() {
        state_         = REQUEST_LINE;
        contentLength_ = 0;
        req_.reset();
    }

private:
    enum State {
        REQUEST_LINE,
        HEADERS,
        BODY,
        COMPLETE
    };

    Result parseRequestLine(const std::string& line);
    Result parseHeader(const std::string& line);

    State       state_;
    size_t      contentLength_;
    HttpRequest req_;
};
