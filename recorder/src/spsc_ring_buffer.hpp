/*
Goal: Implement a thread-safe, bounded ring buffer optimized for single producer 
and single consumer scenarios. 
This data structure should efficiently handle high-throughput scenarios where 
one thread produces data and another thread consumes it.

Need to support generic data types.
Non-blocking operations where possible.

Critical Behaviors:
- Wraparound: Buffer should seamlessly wrap around when reaching the end of the allocated array
- Back-pressure handling: Define behavior when buffer is full
    Option A: Block producer until space available
    Option B: Return failure/false and let caller handle --> do this
    Option C: Overwrite oldest data (specify if this is desired)
*/

#pragma once

#include <atomic>
#include <cassert>
#include <iostream>

template<typename T>
class SPSCRingBuffer {
    public:
        // The explicit keyword is a C++ best practice that prevents implicit conversions.
        explicit SPSCRingBuffer(size_t capacity, bool drop_oldest = false) 
            : buffer_(std::make_unique<T[]>(capacity + 1)),
              capacity_(capacity), drop_oldest_(drop_oldest) {
                assert(capacity > 0);
              }
        
        // Producer operation - returns False if buffer is full
        // and the class is configured to drop newest.
        bool push(const T& item);

        bool pop(T& item);

        size_t size() const;

        size_t capacity() const;

        bool is_empty() const;

        bool is_full() const;
    
    private:
        std::unique_ptr<T[]> buffer_;
        const size_t capacity_;
        bool drop_oldest_;
        alignas(64) std::atomic<size_t> write_index_{0};
        alignas(64) std::atomic<size_t> read_index_{0};
};

// Producer operation - returns False if buffer is full.
template<typename T>
bool SPSCRingBuffer<T>::push(const T& item) {
    const size_t current_write = write_index_.load(std::memory_order_relaxed);
    const size_t next_write = (current_write + 1) % (capacity_ + 1);

    // Check if buffer is full by reading consumer's index
    const size_t current_read = read_index_.load(std::memory_order_acquire);
    if (next_write == current_read && !drop_oldest_) {
        return false;
    }

    // write the data
    buffer_[current_write] = item;

    // make the item available to consumer (release ensures data write completes first)
    write_index_.store(next_write, std::memory_order_release);
    return true;
}

template<typename T>
bool SPSCRingBuffer<T>::pop(T& item) {
    const size_t current_read = read_index_.load(std::memory_order_relaxed);

    // Check if buffer is empty by reading producer's index
    const size_t current_write = write_index_.load(std::memory_order_acquire);
    if (current_write == current_read) {
        return false;
    }

    // read the data
    item = buffer_[current_read];

    // make the item available to producer (release ensures data write completes first)
    const size_t next_read = (current_read + 1) % (capacity_ + 1);
    read_index_.store(next_read, std::memory_order_release);
    return true;
}

template<typename T>
size_t SPSCRingBuffer<T>::size() const {
    const size_t write_idx = write_index_.load(std::memory_order_acquire);
    const size_t read_idx = read_index_.load(std::memory_order_acquire);
    return (write_idx >= read_idx) ? (write_idx - read_idx) : (capacity_ + write_idx - read_idx);
}

template<typename T>
size_t SPSCRingBuffer<T>::capacity() const {
    return capacity_;
}

template<typename T>
bool SPSCRingBuffer<T>::is_empty() const {
    return (
        read_index_.load(std::memory_order_acquire) == 
        write_index_.load(std::memory_order_acquire)
    );
}

template<typename T>
bool SPSCRingBuffer<T>::is_full() const {
    const size_t current_write = write_index_.load(std::memory_order_acquire);
    const size_t next_write = (current_write + 1) % capacity_;
    return next_write == read_index_.load(std::memory_order_acquire);
}

