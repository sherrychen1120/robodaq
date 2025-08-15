#include "metadata_writer.hpp"

bool MetadataWriter::write_metadata(
    const std::string& path,
    const std::unordered_map<std::string, std::unordered_map<std::string, int>>& cam_config,
    int sync_tolerance_us,
    const std::string& front_video_path,
    const std::string& right_video_path,
    const std::string& sync_log_path
) {
    std::ofstream metadata_file(path);
    if (!metadata_file.is_open()) {
        std::cerr << "Failed to create metadata file: " << path << std::endl;
        return false;
    }
    
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    // Write JSON metadata
    metadata_file << "{\n";
    metadata_file << "  \"recording_info\": {\n";
    metadata_file << "    \"timestamp\": \"" << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    metadata_file << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z\",\n";
    metadata_file << "    \"recorder_version\": \"1.0.0\",\n";
    metadata_file << "    \"format_version\": \"1.0.0\"\n";
    metadata_file << "  },\n";
    
    metadata_file << "  \"camera_config\": {\n";
    bool first_cam = true;
    for (const auto& [device_name, config] : cam_config) {
        if (!first_cam) metadata_file << ",\n";
        first_cam = false;
        
        metadata_file << "    \"" << device_name << "\": {\n";
        bool first_param = true;
        for (const auto& [param_name, param_value] : config) {
            if (!first_param) metadata_file << ",\n";
            first_param = false;
            metadata_file << "      \"" << param_name << "\": " << param_value;
        }
        metadata_file << "\n    }";
    }
    metadata_file << "\n  },\n";
    
    metadata_file << "  \"recorder_config\": {\n";
    metadata_file << "    \"sync_tolerance_us\": " << sync_tolerance_us << "\n";
    metadata_file << "  },\n";
    
    metadata_file << "  \"output_files\": {\n";
    metadata_file << "    \"front_camera_video\": \"" << front_video_path << "\",\n";
    metadata_file << "    \"right_camera_video\": \"" << right_video_path << "\",\n";
    metadata_file << "    \"sync_log\": \"" << sync_log_path << "\"\n";
    metadata_file << "  }\n";
    metadata_file << "}\n";
    
    metadata_file.close();
    std::cout << "Metadata written to: " << path << std::endl;
    return true;
}
