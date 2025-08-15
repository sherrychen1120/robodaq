/*

Goal
Implement a bounded blocking queue using std::mutex + std::condition_variable, then measure total ops/sec for:
- Scenario A: 1 producer / 1 consumer
- Scenario B: 1 producer / 2 consumers

*/

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include "../src/mpmc_ring_buffer.hpp"

// [0,1,2,3,4]
// [null, null, null, null, null]

// push to 0
// push_idx, pop_idx
// 1         0
// pop
// 1         1
struct Item { 
    uint64_t seq; 
    char payload[56]; 
};

// Global variables for controlling the threads
std::atomic<bool> stop_producer = false;
std::atomic<bool> stop_consumer = false;
std::atomic<uint64_t> total_produced = 0;
std::atomic<uint64_t> total_consumed = 0;

void producer_thread(MPMCRingBuffer<Item>& ring_buffer, uint64_t num_items) {
    for(uint64_t i = 0; i < num_items && !stop_producer; i++) {
        Item item{i, "Hello"};
        ring_buffer.push(item);
        total_produced++;
        
        // // Optional: add some logging every 1000 items
        // if(i % 1000 == 0) {
        //     std::cout << "Produced: " << i << std::endl;
        // }
    }
    std::cout << "Producer finished. Total produced: " << total_produced << std::endl;
}

void consumer_thread(MPMCRingBuffer<Item>& ring_buffer, uint64_t expected_items) {
    uint64_t consumed = 0;
    while(consumed < expected_items && !stop_consumer) {
        Item item;
        ring_buffer.pop(item);
        consumed++;
        total_consumed++;
        
        // // Optional: add some logging every 1000 items  
        // if(consumed % 1000 == 0) {
        //     std::cout << "Consumed: " << consumed << " (seq=" << item.seq << ")" << std::endl;
        // }
    }
    std::cout << "Consumer finished. Consumed: " << consumed << " Total consumed: " << total_consumed << std::endl;
}

int main() {
    const int buffer_size = 1024;
    const long long num_items = 10'000'000;  // Number of items to produce/consume
    
    MPMCRingBuffer<Item> ring_buffer(buffer_size);
    
    std::cout << "Starting performance test..." << std::endl;
    std::cout << "Buffer size: " << buffer_size << std::endl;
    std::cout << "Items to process: " << num_items << std::endl;
    
    // Scenario A: 1 producer / 1 consumer
    std::cout << "\n=== Scenario A: 1 Producer / 1 Consumer ===" << std::endl;
    
    // Reset counters
    total_produced = 0;
    total_consumed = 0;
    stop_producer = false;
    stop_consumer = false;

    auto start_time = std::chrono::steady_clock::now();
    
    // Create and start threads
    std::thread producer(producer_thread, std::ref(ring_buffer), num_items);
    std::thread consumer(consumer_thread, std::ref(ring_buffer), num_items);
    
    // Wait for both threads to complete
    producer.join();
    consumer.join();
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "\nScenario A Results:" << std::endl;
    std::cout << "Time taken: " << duration.count() << " ms" << std::endl;
    std::cout << "Total operations: " << (total_produced + total_consumed) << std::endl;
    std::cout << "Operations per second: " << (total_produced + total_consumed) * 1000 / duration.count() << std::endl;
    
    // Scenario B: 1 producer / 2 consumers
    std::cout << "\n=== Scenario B: 1 Producer / 2 Consumers ===" << std::endl;
    
    // Reset counters
    total_produced = 0;
    total_consumed = 0;
    stop_producer = false;
    stop_consumer = false;

    start_time = std::chrono::steady_clock::now();
    
    // Create and start threads
    std::thread producer2(producer_thread, std::ref(ring_buffer), num_items);
    std::thread consumer1(consumer_thread, std::ref(ring_buffer), num_items / 2);
    std::thread consumer2(consumer_thread, std::ref(ring_buffer), num_items / 2);
    
    // Wait for all threads to complete
    producer2.join();
    consumer1.join();
    consumer2.join();
    
    end_time = std::chrono::steady_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "\nScenario B Results:" << std::endl;
    std::cout << "Time taken: " << duration.count() << " ms" << std::endl;
    std::cout << "Total operations: " << (total_produced + total_consumed) << std::endl;
    std::cout << "Operations per second: " << (total_produced + total_consumed) * 1000 / duration.count() << std::endl;
    
    return 0;
}
