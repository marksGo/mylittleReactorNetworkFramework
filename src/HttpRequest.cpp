#include "HttpRequest.h"
#include <algorithm>

void HttpRequest::setMethod(const std::string& m) {
    if      (m == "GET")    method_ = GET;
    else if (m == "POST")   method_ = POST;
    else if (m == "PUT")    method_ = PUT;
    else if (m == "DELETE") method_ = DELETE;
    else                    method_ = UNKNOWN;
}

void HttpRequest::setVersion(const std::string& v) {
    if      (v == "HTTP/1.0") version_ = HTTP10;
    else if (v == "HTTP/1.1") version_ = HTTP11;
    else                      version_ = UNKNOWN_VER;
}

std::string HttpRequest::methodStr() const {
    switch (method_) {
        case GET:    return "GET";
        case POST:   return "POST";
        case PUT:    return "PUT";
        case DELETE: return "DELETE";
        default:     return "UNKNOWN";
    }
}

bool HttpRequest::keepAlive() const {
    auto it = headers_.find("Connection");
    if (it == headers_.end()) {
        // HTTP/1.1 默认 keep-alive，HTTP/1.0 默认 close
        return version_ == HTTP11;
    }
    std::string val = it->second;
    std::transform(val.begin(), val.end(), val.begin(), ::tolower);
    return val == "keep-alive";
}
