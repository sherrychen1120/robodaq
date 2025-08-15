#include <atomic>
#include <csignal>
#include <iostream>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <memory>
#include <cstdlib>

#include "camera_capture_pipeline.hpp"
#include "spsc_ring_buffer.hpp"

// used for synchronizing frames between the two cameras.
// Camera frames more than this duration apart won't be considered
// in the same recorder frame. (in microseconds)
// TODO(sherry): Just make this the frame_rate of cam_front if we 
// are using that as the trigger?
constexpr int SYNC_TOLERANCE_US = 1'000'000 / 30.0;
std::unordered_map<std::string, std::unordered_map<std::string, int>> CAM_CONFIG = {
    {
        "/dev/cam_front", 
        {
            {"width", 640},
            {"height", 480},
            {"frame_rate", 30}
        }
    },
    {
        "/dev/cam_right", 
        {
            {"width", 640},
            {"height", 480},
            {"frame_rate", 30}
        }
    }
};

// Global flag for signal handling - needs to be accessible from static signal handler
static volatile sig_atomic_t keep_running = 1;

// Static signal handler function
static void signal_handler(int signal) {
    keep_running = 0;
}

class Recorder {
    private:
        // atomic flag for sleep polling
        std::atomic<bool> should_tick{false};
        
        // Ring buffers for each camera (storing CameraFrame objects)
        std::unique_ptr<SPSCRingBuffer<CameraFrame>> front_buffer;
        std::unique_ptr<SPSCRingBuffer<CameraFrame>> right_buffer;
        
        // Synchronization thread
        std::unique_ptr<std::thread> sync_thread;
        
        // Frame rate for synchronization timing
        int sync_tolerance_us_;

        // Unified camera frame callback
        void on_camera_frame(const CameraFrame& frame, bool trigger_record) {
            std::cout << "[" << frame.device_name << "] Frame seq:" << frame.sequence_number 
                      << " timestamp:" << frame.timestamp_us 
                      << " size:" << frame.image_data.size() << " bytes" << std::endl;
            
            // Store frame in appropriate buffer
            if (frame.device_name == "/dev/cam_front") {
                if (front_buffer && !front_buffer->push(frame)) {
                    std::cerr << "[FRONT] Ring buffer full, dropping frame" << std::endl;
                }
                // Trigger recording if this is the front camera with trigger_record=true
                if (trigger_record) {
                    should_tick.store(true);
                }
            } else if (frame.device_name == "/dev/cam_right") {
                if (right_buffer && !right_buffer->push(frame)) {
                    std::cerr << "[RIGHT] Ring buffer full, dropping frame" << std::endl;
                }
            }
        }
        
        // Synchronization thread function
        void sync_thread_func() {
            const auto poll_interval = std::chrono::microseconds(100); // 10kHz polling
            
            while (keep_running) {
                if (should_tick.load()) {
                    should_tick.store(false);
                    
                    // Try to get front frame
                    CameraFrame front_frame;
                    if (front_buffer && front_buffer->pop(front_frame)) {
                        // Look for matching right frame
                        CameraFrame right_frame;
                        bool found_match = false;
                        
                        // Keep popping right frames until we find one within tolerance
                        uint64_t time_diff = 0;
                        while (right_buffer && right_buffer->pop(right_frame)) {
                            time_diff = std::abs(static_cast<int64_t>(right_frame.timestamp_us) - static_cast<int64_t>(front_frame.timestamp_us));
                            
                            if (time_diff <= sync_tolerance_us_) {
                                found_match = true;
                                break;
                            }
                            // If right frame is too old, continue to next one
                            // If right frame is too new, we missed the sync window
                            if (right_frame.timestamp_us > front_frame.timestamp_us + sync_tolerance_us_) {
                                break;
                            }
                        }
                        
                        if (found_match) {
                            std::cout << "SYNC: Seq=" << front_frame.sequence_number 
                                      << " Front ts=" << front_frame.timestamp_us 
                                      << " Right ts=" << right_frame.timestamp_us
                                      << " diff=" << time_diff
                                      << "us" << " < tol=" << sync_tolerance_us_ << "us" << std::endl;
                        } else {
                            std::cout << "SYNC: No matching right frame for front ts=" << front_frame.timestamp_us << std::endl;
                        }
                    }
                }
                
                std::this_thread::sleep_for(poll_interval);
            }
        }

        bool start_pipeline(
            CameraPipeline& pipeline,
            const std::string& device_name,
            std::unordered_map<std::string, int> cam_config,
            SinkMode mode,
            FrameCallback callback = nullptr,
            bool trigger_record_flag = false,
            bool enable_fps_debug = false
        ) {
            // Initialize with specific video parameters: 640x480 @ 30fps
            if (!pipeline.initialize(
                device_name, cam_config["width"], cam_config["height"], 
                cam_config["frame_rate"], mode, callback, trigger_record_flag, enable_fps_debug
            )) {
                std::cerr << "Failed to initialize camera pipeline for " << device_name << std::endl;
                return false;
            }
        
            std::cout << "Camera pipeline " << device_name << " initialized successfully!" << std::endl;
        
            if (!pipeline.start()) {
                std::cerr << "Failed to start camera pipeline" << std::endl;
                return false;
            }
        
            std::cout << "Camera pipeline " << device_name << " started! Press Ctrl+C to stop..." << std::endl;
            return true;
        }
    
    public:
        Recorder() : sync_tolerance_us_(SYNC_TOLERANCE_US) {
            // Initialize ring buffers (capacity of 100 frames each)
            front_buffer = std::make_unique<SPSCRingBuffer<CameraFrame>>(100);
            right_buffer = std::make_unique<SPSCRingBuffer<CameraFrame>>(100);
        }
        
        bool run(SinkMode mode) {
            // Set up signal handler for graceful shutdown
            signal(SIGINT, signal_handler);
            signal(SIGTERM, signal_handler);
            
            CameraPipeline pipeline_front;
            CameraPipeline pipeline_right;

            // Create unified callback using lambda
            auto camera_callback = [this](const CameraFrame& frame, bool trigger_record) {
                this->on_camera_frame(frame, trigger_record);
            };

            // Triggering recording on front camera.
            if (!(
                start_pipeline(
                    pipeline_front, "/dev/cam_front", CAM_CONFIG["/dev/cam_front"],
                    mode, camera_callback, /* trigger_record */ true
                ) && start_pipeline(
                    pipeline_right, "/dev/cam_right", CAM_CONFIG["/dev/cam_right"],
                    mode, camera_callback, /* trigger_record */ false 
                )
            )) {
                return false;
            }
            
            // Start synchronization thread
            sync_thread = std::make_unique<std::thread>(&Recorder::sync_thread_func, this);
            std::cout << "Synchronization thread started (polling at 10kHz)" << std::endl;
            
            // Keep the program running
            while (keep_running) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            std::cout << "\nShutting down..." << std::endl;
            
            // Wait for sync thread to finish
            if (sync_thread && sync_thread->joinable()) {
                sync_thread->join();
            }
            
            pipeline_front.stop();
            pipeline_right.stop();

            return true;
        }
};
