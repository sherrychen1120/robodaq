#pragma once
#include <vector>
#include <atomic>
template<typename T>
class SpscRingBuffer {
public:
    explicit SpscRingBuffer(size_t capacity) : buf_(capacity), cap_(capacity) {}
    bool push(const T& v) {
        size_t next = (head_ + 1) % cap_;
        if (next == tail_.load(std::memory_order_acquire)) return false;
        buf_[head_] = v;
        head_ = next;
        return true;
    }
    bool pop(T& out) {
        size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_) return false;
        out = buf_[tail];
        tail = (tail + 1) % cap_;
        tail_.store(tail, std::memory_order_release);
        return true;
    }
private:
    std::vector<T> buf_;
    size_t cap_;
    size_t head_{0};
    std::atomic<size_t> tail_{0};
};
