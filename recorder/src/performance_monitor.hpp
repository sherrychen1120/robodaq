#pragma once

#include <fstream>
#include <string>
#include <iostream>
#include <unordered_map>
#include <sstream>
#include <chrono>
#include <iomanip>

struct FrameData {
    uint64_t timestamp_us;
    uint64_t sequence_number;
    int latency_us;
};

class PerformanceMonitor {
private:
    std::unordered_map<std::string, uint64_t> last_seq_num_by_device_;
    std::unordered_map<std::string, double> mean_latency_by_device_;
    std::unordered_map<std::string, int> latency_sample_count_by_device_;
    std::unordered_map<std::string, int> seq_gap_count_by_device_;
    int num_frames_;
    std::string events_output_path_;
    std::string output_dir_;
    std::ofstream events_file_;

    void log_sequence_gap_event_(
        const std::string& device_name,
        const FrameData& frame_data,
        uint64_t gap
    );
    void update_latency_average_(const std::string& device_name, int latency_us);

public:
    PerformanceMonitor() : num_frames_(0) {}
    bool initialize(const std::string& output_dir);
    void tick(const std::unordered_map<std::string, FrameData>& frame_data_by_device);
    void report();
    void print_live_metrics() const;
    
};

