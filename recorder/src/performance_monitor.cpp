#include "performance_monitor.hpp"

bool PerformanceMonitor::initialize(const std::string& output_dir) {
    output_dir_ = output_dir;
    events_output_path_ = output_dir + "/events.jsonl";
    events_file_.open(events_output_path_, std::ios::out | std::ios::trunc);
    
    if (!events_file_.is_open()) {
        std::cerr << "Failed to open events file: " << events_output_path_ << std::endl;
        return false;
    }
    
    std::cout << "PerformanceMonitor initialized: " << events_output_path_ << std::endl;
    return true;
}

void PerformanceMonitor::tick(
    const std::unordered_map<std::string, FrameData>& frame_data_by_device
) {
    num_frames_++;
    
    for (const auto& pair : frame_data_by_device) {
        const std::string& device_name = pair.first;
        const FrameData& frame_data = pair.second;

        // Update rolling average latency
        update_latency_average_(device_name, frame_data.latency_us);
        
        // Check gap in sequence number
        if (last_seq_num_by_device_.find(device_name) != last_seq_num_by_device_.end()) {
            uint64_t last_seq_num = last_seq_num_by_device_[device_name];
            uint64_t gap = frame_data.sequence_number - last_seq_num;
            if (gap > 1) {
                log_sequence_gap_event_(device_name, frame_data, gap - 1);
                seq_gap_count_by_device_[device_name]++;
            }
        }
        
        // Update last sequence number
        last_seq_num_by_device_[device_name] = frame_data.sequence_number;
    }
}

void PerformanceMonitor::log_sequence_gap_event_(
    const std::string& device_name,
    const FrameData& frame_data,
    uint64_t gap
) {
    if (!events_file_.is_open()) {
        std::cerr << "PerformanceMonitor events file not initialized" << std::endl;
        return;
    }
    
    // Create JSON line
    std::ostringstream json_line;
    json_line << "{"
              << "\"timestamp_us\":" << frame_data.timestamp_us << ","
              << "\"event_type\":\"sequence_gap\","
              << "\"device_name\":\"" << device_name << "\","
              << "\"sequence_number\":" << frame_data.sequence_number << ","
              << "\"gap_size\":" << gap
              << "}" << std::endl;
    
    events_file_ << json_line.str();
    events_file_.flush();  // Ensure data is written immediately

    std::cout << "SEQ GAP: " << device_name << " ts=" << frame_data.timestamp_us << " seq=" << frame_data.sequence_number 
              << " gap=" << gap << std::endl;
}

void PerformanceMonitor::update_latency_average_(const std::string& device_name, int latency_us) {
    if (mean_latency_by_device_.find(device_name) == mean_latency_by_device_.end()) {
        // First sample for this device
        mean_latency_by_device_[device_name] = latency_us;
        latency_sample_count_by_device_[device_name] = 1;
    } else {
        // Update running average using incremental formula: new_avg = old_avg + (new_value - old_avg) / count
        int count = ++latency_sample_count_by_device_[device_name];
        double old_avg = mean_latency_by_device_[device_name];
        mean_latency_by_device_[device_name] = old_avg + (latency_us - old_avg) / count;
    }
}

void PerformanceMonitor::report() {
    std::cout << "\n=== Performance Report ===" << std::endl;
    std::cout << "Total frames processed: " << num_frames_ << std::endl;
    
    // Print mean latency by device
    std::cout << "\nMean Latency by Device:" << std::endl;
    for (const auto& pair : mean_latency_by_device_) {
        std::cout << "  " << pair.first << ": " << std::fixed << std::setprecision(2) 
                  << pair.second << " us (" << latency_sample_count_by_device_.at(pair.first) 
                  << " samples)" << std::endl;
    }
    
    // Print sequence gaps by device
    std::cout << "\nSequence Gaps by Device:" << std::endl;
    for (const auto& pair : seq_gap_count_by_device_) {
        std::cout << "  " << pair.first << ": " << pair.second << " gaps" << std::endl;
    }
    
    // Write metrics to JSON file
    std::string metrics_path = output_dir_ + "/metrics.json";
    std::ofstream metrics_file(metrics_path);
    if (metrics_file.is_open()) {
        metrics_file << "{" << std::endl;
        metrics_file << "  \"total_frames\": " << num_frames_ << "," << std::endl;
        metrics_file << "  \"mean_latency_by_device\": {" << std::endl;
        
        bool first_latency = true;
        for (const auto& pair : mean_latency_by_device_) {
            if (!first_latency) metrics_file << "," << std::endl;
            metrics_file << "    \"" << pair.first << "\": {"
                        << "\"mean_latency_us\": " << std::fixed << std::setprecision(2) << pair.second
                        << ", \"sample_count\": " << latency_sample_count_by_device_.at(pair.first) << "}";
            first_latency = false;
        }
        metrics_file << std::endl << "  }," << std::endl;
        
        metrics_file << "  \"sequence_gaps_by_device\": {" << std::endl;
        bool first_gap = true;
        for (const auto& pair : seq_gap_count_by_device_) {
            if (!first_gap) metrics_file << "," << std::endl;
            metrics_file << "    \"" << pair.first << "\": " << pair.second;
            first_gap = false;
        }
        metrics_file << std::endl << "  }" << std::endl;
        metrics_file << "}" << std::endl;
        
        metrics_file.close();
        std::cout << "\nMetrics written to: " << metrics_path << std::endl;
    } else {
        std::cerr << "Failed to write metrics file: " << metrics_path << std::endl;
    }
    
    std::cout << "========================\n" << std::endl;
}
