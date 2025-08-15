#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <vector>

template<typename T>
class MPMCRingBuffer {
public:
    std::mutex mtx;
    std::condition_variable not_full;   // Signals when buffer is not full
    std::condition_variable not_empty;  // Signals when buffer is not empty

    MPMCRingBuffer(int size) {
        buffer.resize(size);
    }
    
    // Blocking push - waits until buffer has space
    // Lock acquired at function start
    // Temporarily unlocked during wait() (if blocking)
    // Re-locked when woken up
    // Buffer modification happens while locked
    // Unlock happens at function end via RAII
    void push(const T& item) {
        // You must use unique_lock with condition variables because they 
        // need the ability to unlock and re-lock the mutex during the wait operation.
        // lock_guard simply doesn't have this capability!
        std::unique_lock<std::mutex> lock(mtx);
        
        // Wait until buffer is not full
        not_full.wait(lock, [this] { 
            return !(push_idx == pop_idx && buffer[push_idx].has_value()); 
        });
        
        // Now we have space - add the item
        buffer[push_idx] = item;
        push_idx = (push_idx + 1) % buffer.size();
        
        // Notify consumers that buffer is not empty
        not_empty.notify_one();
    } // unlocked mutex here when lock destructor runs

    // Blocking pop - waits until buffer has data
    void pop(T& item) {
        std::unique_lock<std::mutex> lock(mtx);
        
        // Wait until buffer is not empty
        not_empty.wait(lock, [this] {
            return push_idx != pop_idx || buffer[pop_idx].has_value();
        });
        
        // Now we have data - extract the item
        item = buffer[pop_idx].value();
        buffer[pop_idx] = std::nullopt;
        pop_idx = (pop_idx + 1) % buffer.size();
        
        // Notify producers that buffer is not full
        not_full.notify_one();
    } // unlocked mutex here when lock destructor runs

private:
    std::vector<std::optional<T>> buffer; // 1024
    int push_idx = 0; // push to idx 0
    int pop_idx = 0;
};
