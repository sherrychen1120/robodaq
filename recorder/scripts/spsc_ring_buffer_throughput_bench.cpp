#include "../src/spsc_ring_buffer.hpp"
#include <iostream>
#include <thread>

struct Item {
    uint64_t seq;
    char payload[56];
};

std::atomic<uint64_t> total_produced;
std::atomic<uint64_t> total_consumed;

void producer_thread(std::string name, SPSCRingBuffer<Item>& ring_buffer, uint64_t num_items) {
    uint64_t produced = 0;
    while (produced < num_items) {
        // std::cout << "produced: " << produced << std::endl;
        Item item{produced, "hello"};
        ring_buffer.push(item);
        produced++;
        total_produced++;
    }
    std::cout << "Producer " << name << " finished. Produced: " << produced << std::endl;
}

void consumer_thread(std::string name, SPSCRingBuffer<Item>& ring_buffer, uint64_t num_expected) {
    uint64_t consumed = 0;
    while (consumed < num_expected) {
        // std::cout << "consumed: " << consumed << std::endl;
        Item item;
        ring_buffer.pop(item);
        consumed++;
        total_consumed++;
    }
    std::cout << "Consumer " << name << " finished. Consumed: " << consumed << std::endl;
}

int main()
{
    int N = 1024;
    uint64_t total_ops = 10'000'000;
    
    // 1 producer, 1 consumer
    std::cout << "Scenario 1: 1 producer / 1 consumer " << std::endl;
    SPSCRingBuffer<Item> ring_buffer(N, true); // drop oldest
    total_produced = 0;
    total_consumed = 0;
    
    auto start_time = std::chrono::steady_clock::now();
    std::thread producer(producer_thread, "p1", std::ref(ring_buffer), total_ops);
    std::thread consumer(consumer_thread, "c1", std::ref(ring_buffer), total_ops);
    
    producer.join();
    consumer.join();
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
    auto ops_per_sec = (total_produced + total_consumed) * 1e9 / duration.count();
    
    std::cout << "Total produced: " << total_produced << std::endl;
    std::cout << "Total consumed: " << total_consumed << std::endl;
    std::cout << "Ops per sec: " << ops_per_sec << std::endl;

    return 0;
}
