#include "sync_logger.hpp"

SyncLogger::SyncLogger() {}

bool SyncLogger::initialize(const std::string& path) {
    output_path_ = path;
    log_file_.open(path, std::ios::out | std::ios::trunc);
    
    if (!log_file_.is_open()) {
        std::cerr << "Failed to open sync log file: " << path << std::endl;
        return false;
    }
    
    std::cout << "SyncLogger initialized: " << path << std::endl;
    return true;
}

void SyncLogger::log_sync_event(uint64_t timestamp_us, uint64_t cam1_frame_id, uint64_t cam2_frame_id, uint64_t seq_num) {
    if (!log_file_.is_open()) {
        std::cerr << "SyncLogger not initialized" << std::endl;
        return;
    }
    
    // Create JSON line
    std::ostringstream json_line;
    json_line << "{"
              << "\"timestamp\":" << timestamp_us << ","
              << "\"cam1_frame_id\":" << cam1_frame_id << ","
              << "\"cam2_frame_id\":" << cam2_frame_id << ","
              << "\"seq_num\":" << seq_num
              << "}" << std::endl;
    
    log_file_ << json_line.str();
    log_file_.flush();  // Ensure data is written immediately
    
    // std::cout << "SYNC LOG: " << json_line.str().substr(0, json_line.str().length() - 1) << std::endl;
}

void SyncLogger::finalize() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
    std::cout << "SyncLogger finalized: " << output_path_ << std::endl;
}

SyncLogger::~SyncLogger() {
    finalize();
}
