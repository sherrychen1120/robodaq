#pragma once

#include <fstream>
#include <string>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <unordered_map>

class MetadataWriter {
public:
    static bool write_metadata(
        const std::string& path,
        const std::unordered_map<std::string, std::unordered_map<std::string, int>>& cam_config,
        int sync_tolerance_us,
        const std::string& front_video_path,
        const std::string& right_video_path,
        const std::string& sync_log_path
    );
};

