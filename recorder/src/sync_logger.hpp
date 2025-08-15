#pragma once

#include <fstream>
#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdint>

struct SyncEvent {
    uint64_t timestamp_us;
    uint64_t cam1_frame_id;  // front camera sequence number
    uint64_t cam2_frame_id;  // right camera sequence number
    uint64_t seq_num;        // sync sequence number
};

class SyncLogger {
private:
    std::ofstream log_file_;
    std::string output_path_;
    
public:
    SyncLogger();
    
    bool initialize(const std::string& path);
    void log_sync_event(uint64_t timestamp_us, uint64_t cam1_frame_id, uint64_t cam2_frame_id, uint64_t seq_num);
    void finalize();
    
    ~SyncLogger();
};

