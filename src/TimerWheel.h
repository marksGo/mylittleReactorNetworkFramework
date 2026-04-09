#pragma once
#include <functional>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>

// 简单时间轮：每秒 tick 一次，支持秒级超时
// 用于踢掉长时间无数据的空连接
class TimerWheel {
public:
    using TimeoutCb = std::function<void()>;

    // slots: 轮槽数（= 最大超时秒数），tickInterval: 每格秒数
    explicit TimerWheel(int slots = 60);
    ~TimerWheel() = default;

    // 注册超时（delaySec 秒后触发 cb）
    // 返回 token，可用于 cancel
    int  addTimer(int delaySec, TimeoutCb cb);
    void cancelTimer(int token);

    // 每秒调用一次（由外部定时触发，例如 timerfd + EventLoop）
    void tick();

private:
    struct Timer {
        int       token;
        int       rounds;  // 还需转几圈
        TimeoutCb cb;
    };

    int slots_;
    int current_;   // 当前指针
    std::vector<std::vector<std::shared_ptr<Timer>>> wheel_;
    std::unordered_map<int, std::shared_ptr<Timer>>  tokenMap_;
    int nextToken_;
    std::mutex mutex_;
};
