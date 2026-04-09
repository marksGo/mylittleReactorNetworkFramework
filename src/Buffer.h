#pragma once
#include <vector>
#include <string>
#include <cstring>
#include <cassert>

// 简单的动态缓冲区，支持自动扩容
class Buffer {
public:
    static const size_t kInitialSize = 4096;
    static const size_t kPrepend     = 8;   // 头部预留（可用于存长度）

    explicit Buffer(size_t size = kInitialSize)
        : buf_(kPrepend + size), readIdx_(kPrepend), writeIdx_(kPrepend) {}

    // 可读字节数
    size_t readable() const { return writeIdx_ - readIdx_; }
    // 可写字节数
    size_t writable() const { return buf_.size() - writeIdx_; }
    // 预留区大小
    size_t prepend() const { return readIdx_; }

    const char* peek() const { return begin() + readIdx_; }
    char*       beginWrite()  { return begin() + writeIdx_; }

    // 消费 len 字节
    void retrieve(size_t len) {
        assert(len <= readable());
        if (len < readable()) readIdx_ += len;
        else                  retrieveAll();
    }
    void retrieveAll() { readIdx_ = writeIdx_ = kPrepend; }

    // 取出所有可读数据为 string
    std::string retrieveAllAsString() {
        std::string s(peek(), readable());
        retrieveAll();
        return s;
    }

    // 追加数据
    void append(const char* data, size_t len) {
        ensureWritable(len);
        std::memcpy(beginWrite(), data, len);
        writeIdx_ += len;
    }
    void append(const std::string& s) { append(s.data(), s.size()); }

    // 从 fd 读数据（配合 ET 模式，一次读完）
    ssize_t readFd(int fd, int* savedErrno);

private:
    void ensureWritable(size_t len) {
        if (writable() >= len) return;
        // 先尝试移动数据到头部
        if (writable() + prepend() - kPrepend >= len) {
            size_t r = readable();
            std::memmove(begin() + kPrepend, peek(), r);
            readIdx_  = kPrepend;
            writeIdx_ = kPrepend + r;
        } else {
            buf_.resize(writeIdx_ + len);
        }
    }

    char*       begin()       { return buf_.data(); }
    const char* begin() const { return buf_.data(); }

    std::vector<char> buf_;
    size_t readIdx_;
    size_t writeIdx_;
};
