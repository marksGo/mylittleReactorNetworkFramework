#include "Buffer.h"
#include <sys/uio.h>
#include <errno.h>
#include <unistd.h>

// ET 模式下需要一次把数据全部读完
// 用 readv + 栈上备用缓冲区，避免频繁扩容
ssize_t Buffer::readFd(int fd, int* savedErrno) {
    char   extra[65536];   // 栈上备用区
    struct iovec vec[2];

    const size_t writable = this->writable();
    vec[0].iov_base = beginWrite();
    vec[0].iov_len  = writable;
    vec[1].iov_base = extra;
    vec[1].iov_len  = sizeof(extra);

    // 如果 writable 已经够大，就只用一个 iov
    const int iovcnt = (writable < sizeof(extra)) ? 2 : 1;
    ssize_t n = ::readv(fd, vec, iovcnt);
    if (n < 0) {
        *savedErrno = errno;
    } else if (static_cast<size_t>(n) <= writable) {
        writeIdx_ += static_cast<size_t>(n);
    } else {
        writeIdx_ = buf_.size();
        append(extra, static_cast<size_t>(n) - writable);
    }
    return n;
}
