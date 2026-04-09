#pragma once
#include <string>
#include <unordered_map>

class HttpRequest {
public:
    enum Method { GET, POST, PUT, DELETE, UNKNOWN };
    enum Version { HTTP10, HTTP11, UNKNOWN_VER };

    HttpRequest() : method_(UNKNOWN), version_(UNKNOWN_VER) {}

    void   setMethod(const std::string& m);
    void   setUrl(const std::string& url)     { url_ = url; }
    void   setVersion(const std::string& v);
    void   setHeader(const std::string& k, const std::string& v) { headers_[k] = v; }
    void   setBody(const std::string& b)      { body_ = b; }
    void   setQuery(const std::string& q)     { query_ = q; }

    Method      method()  const { return method_; }
    std::string methodStr() const;
    std::string url()     const { return url_; }
    std::string query()   const { return query_; }
    std::string body()    const { return body_; }
    Version     version() const { return version_; }
    bool        keepAlive() const;

    std::string header(const std::string& k) const {
        auto it = headers_.find(k);
        return it != headers_.end() ? it->second : "";
    }

    const std::unordered_map<std::string, std::string>& headers() const {
        return headers_;
    }

    void reset() {
        method_  = UNKNOWN;
        version_ = UNKNOWN_VER;
        url_.clear(); query_.clear(); body_.clear(); headers_.clear();
    }

private:
    Method      method_;
    Version     version_;
    std::string url_;
    std::string query_;
    std::string body_;
    std::unordered_map<std::string, std::string> headers_;
};
