#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <algorithm>
#include <csignal>
#include <unordered_map>

#include "camera_capture_pipeline.hpp"
#include "recorder.hpp"

// Global flag for graceful shutdown
// sig_atomic_t: integer type which can be accessed as an atomic entity even 
// in the presence of asynchronous interrupts made by signals.
// volatile sig_atomic_t keep_running = 1;

// void signal_handler(int signal) {
//     keep_running = 0;
// }

// static const char* HELP = R"HELP(
// recorder — skeleton
// Usage:
//   recorder --cams cam_front,cam_side --fps 30 --duration 120
// )HELP";

// int main(int argc, char** argv) {
//   std::vector<std::string> args(argv+1, argv+argc);
//   if (args.empty() || std::find(args.begin(), args.end(), std::string("--help")) != args.end()) {
//     std::cout << HELP << std::endl;
//     return 0;
//   }
//   std::cout << "[recorder] Starting (skeleton). Args:" << std::endl;
//   for (auto& a : args) std::cout << "  " << a << std::endl;
//   using clock = std::chrono::steady_clock;
//   auto period = std::chrono::milliseconds(33);
//   auto start = clock::now();
//   for (int i=0; i<60; ++i) {
//     auto now = clock::now();
//     auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count();
//     std::cout << "[recorder] tick " << i << " t_ns=" << ns << std::endl;
//     std::this_thread::sleep_until(start + (i+1)*period);
//   }
//   std::cout << "[recorder] Done." << std::endl;
//   return 0;
// }

// std::unordered_map<std::string, std::unordered_map<std::string, int>> CAM_CONFIG = {
//     {
//         "/dev/cam_front", 
//         {
//             {"width", 800},
//             {"height", 600},
//             {"frame_rate", 30}
//         }
//     },
//     {
//         "/dev/cam_right", 
//         {
//             {"width", 800},
//             {"height", 600},
//             {"frame_rate", 30}
//         }
//     }
// };

// bool start_pipeline(
//     CameraPipeline& pipeline,
//     const std::string& device_name,
//     std::unordered_map<std::string, int> cam_config,
//     bool enable_fps_debug = false
// ) {
//     // Initialize with specific video parameters: 640x480 @ 30fps
//     if (!pipeline.initialize(
//         device_name, cam_config["width"], cam_config["height"], 
//         cam_config["frame_rate"], enable_fps_debug
//     )) {
//         std::cerr << "Failed to initialize camera pipeline for " << device_name << std::endl;
//         return false;
//     }

//     std::cout << "Camera pipeline " << device_name << " initialized successfully!" << std::endl;

//     if (!pipeline.start()) {
//         std::cerr << "Failed to start camera pipeline" << std::endl;
//         return false;
//     }

//     std::cout << "Camera pipeline " << device_name << " started! Press Ctrl+C to stop..." << std::endl;
//     return true;
// }

// int main(int argc, char** argv) {
//     // Set up signal handler for graceful shutdown
//     signal(SIGINT, signal_handler);
//     signal(SIGTERM, signal_handler);
    
//     CameraPipeline pipeline_front;
//     CameraPipeline pipeline_right;

//     if (!(
//         start_pipeline(
//             pipeline_front, "/dev/cam_front", CAM_CONFIG["/dev/cam_front"]
//         ) && start_pipeline(
//             pipeline_right, "/dev/cam_right", CAM_CONFIG["/dev/cam_right"]
//         )
//     )) {
//         return 1;
//     }
    
//     // Keep the program running
//     while (keep_running) {
//         std::this_thread::sleep_for(std::chrono::milliseconds(100));
//     }
    
//     std::cout << "\nShutting down..." << std::endl;
//     pipeline_front.stop();
//     pipeline_right.stop();
    
//     return 0;
// }

int main(int argc, char** argv) {
    Recorder recorder;
    
    // Default to APPSINK mode, but allow DISPLAY mode with --display flag
    SinkMode mode = SinkMode::APPSINK;
    if (argc > 1 && std::string(argv[1]) == "--display") {
        mode = SinkMode::DISPLAY;
    }
    
    if (!recorder.run(mode)) {
        return 1;
    }
    return 0;
}