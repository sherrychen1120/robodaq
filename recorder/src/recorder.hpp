#pragma once

#include <atomic>
#include <csignal>
#include <iostream>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <memory>
#include <cstdlib>
#include <iomanip>
#include <sstream>

#include "camera_capture_pipeline.hpp"
#include "spsc_ring_buffer.hpp"
#include "video_writer.hpp"
#include "sync_logger.hpp"
#include "metadata_writer.hpp"

// used for synchronizing frames between the two cameras.
// Camera frames more than this duration apart won't be considered
// in the same recorder frame. (in microseconds)
// TODO(sherry): Just make this the frame_rate of cam_front if we 
// are using that as the trigger?
constexpr int SYNC_TOLERANCE_US = 1'000'000 / 30.0;

extern std::unordered_map<std::string, std::unordered_map<std::string, int>> CAM_CONFIG;

// Global flag for signal handling - needs to be accessible from static signal handler
extern volatile sig_atomic_t keep_running;

// Static signal handler function
void signal_handler(int signal);

class Recorder {
    private:
        // atomic flag for sleep polling
        std::atomic<bool> should_tick_{false};
        
        // Ring buffers for each camera (storing CameraFrame objects)
        std::unique_ptr<SPSCRingBuffer<CameraFrame>> front_buffer_;
        std::unique_ptr<SPSCRingBuffer<CameraFrame>> right_buffer_;
        
        // Synchronization thread
        std::unique_ptr<std::thread> sync_thread_;
        
        // Frame rate for synchronization timing
        int sync_tolerance_us_;
        
        // Video writers and sync logger
        std::unique_ptr<VideoWriter> front_video_writer_;
        std::unique_ptr<VideoWriter> right_video_writer_;
        std::unique_ptr<SyncLogger> sync_logger_;

        std::string output_dir_;

        // Unified camera frame callback
        void on_camera_frame(const CameraFrame& frame, bool trigger_record);
        
        // Synchronization thread function
        void sync_thread_func();

        bool start_pipeline(
            CameraPipeline& pipeline,
            const std::string& device_name,
            std::unordered_map<std::string, int> cam_config,
            SinkMode mode,
            FrameCallback callback = nullptr,
            bool trigger_record_flag = false,
            bool enable_fps_debug = false
        );
    
    public:
        Recorder(const std::string& output_dir);
        
        bool run(SinkMode mode);
};
