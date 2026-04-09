#include "TimerWheel.h"
#include <algorithm>

TimerWheel::TimerWheel(int slots)
    : slots_(slots), current_(0), wheel_(slots), nextToken_(1) {}

int TimerWheel::addTimer(int delaySec, TimeoutCb cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto t = std::make_shared<Timer>();
    t->token  = nextToken_++;
    t->rounds = delaySec / slots_;
    t->cb     = std::move(cb);

    int slot = (current_ + delaySec % slots_) % slots_;
    wheel_[slot].push_back(t);
    tokenMap_[t->token] = t;
    return t->token;
}

void TimerWheel::cancelTimer(int token) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tokenMap_.find(token);
    if (it == tokenMap_.end()) return;
    it->second->cb = nullptr;   // 置空 cb，tick 时跳过
    tokenMap_.erase(it);
}

void TimerWheel::tick() {
    std::vector<std::shared_ptr<Timer>> expired;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_ = (current_ + 1) % slots_;
        auto& slot = wheel_[current_];
        std::vector<std::shared_ptr<Timer>> remaining;
        for (auto& t : slot) {
            if (!t->cb) continue;      // 已取消
            if (t->rounds <= 0) {
                expired.push_back(t);
                tokenMap_.erase(t->token);
            } else {
                --t->rounds;
                remaining.push_back(t);
            }
        }
        slot = std::move(remaining);
    }
    // 在锁外执行回调
    for (auto& t : expired) {
        if (t->cb) t->cb();
    }
}
