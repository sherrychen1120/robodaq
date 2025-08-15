#include "recorder.hpp"

// Global variables
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
volatile sig_atomic_t keep_running = 1;

// Static signal handler function
void signal_handler(int signal) {
    keep_running = 0;
}

// Recorder constructor
Recorder::Recorder(const std::string& output_dir) 
    : sync_tolerance_us_(SYNC_TOLERANCE_US), output_dir_(output_dir) {
    // Initialize ring buffers (capacity of 100 frames each)
    front_buffer_ = std::make_unique<SPSCRingBuffer<CameraFrame>>(100);
    right_buffer_ = std::make_unique<SPSCRingBuffer<CameraFrame>>(100);
    
    // Initialize video writers and sync logger
    front_video_writer_ = std::make_unique<VideoWriter>();
    right_video_writer_ = std::make_unique<VideoWriter>();
    sync_logger_ = std::make_unique<SyncLogger>();
    performance_monitor_ = std::make_unique<PerformanceMonitor>();
}

// Unified camera frame callback
void Recorder::on_camera_frame(const CameraFrame& frame, bool trigger_record) {    
    // Store frame in appropriate buffer for synchronization
    if (frame.device_name == "/dev/cam_front") {
        if (front_buffer_ && !front_buffer_->push(frame)) {
            std::cerr << "[FRONT] Ring buffer full, dropping frame" << std::endl;
        }
        // Trigger recording if this is the front camera with trigger_record=true
        if (trigger_record) {
            should_tick_.store(true);
        }
    } else if (frame.device_name == "/dev/cam_right") {
        if (right_buffer_ && !right_buffer_->push(frame)) {
            std::cerr << "[RIGHT] Ring buffer full, dropping frame" << std::endl;
        }
    }
}

// Synchronization thread function
void Recorder::sync_thread_func() {
    const auto poll_interval = std::chrono::microseconds(100); // 10kHz polling
    
    while (keep_running) {
        if (should_tick_.load()) {
            should_tick_.store(false);
            
            // Try to get front frame
            CameraFrame front_frame;
            CameraFrame right_frame;
            if (front_buffer_ && front_buffer_->pop(front_frame)) {
                // Look for matching right frame
                bool found_match = false;
                
                // Keep popping right frames until we find one within tolerance
                uint64_t time_diff = 0;
                while (right_buffer_ && right_buffer_->pop(right_frame)) {
                    time_diff = std::abs(
                        static_cast<int64_t>(right_frame.timestamp_us) 
                        - static_cast<int64_t>(front_frame.timestamp_us)
                    );
                    
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
                    // std::cout << "SYNC: Seq=" << front_frame.sequence_number 
                    //           << " Front ts=" << front_frame.timestamp_us 
                    //           << " Right ts=" << right_frame.timestamp_us
                    //           << " diff=" << time_diff
                    //           << "us" << " < tol=" << sync_tolerance_us_ << "us" << std::endl;
                    
                    // Write frame to video file
                    int latency_front, latency_right;
                    front_video_writer_->write_frame(front_frame, latency_front);
                    right_video_writer_->write_frame(right_frame, latency_right);

                    // Log sync event to JSONL file
                    if (sync_logger_) {
                        sync_logger_->log_sync_event(
                            front_frame.timestamp_us,
                            front_frame.sequence_number,
                            right_frame.sequence_number,
                            front_frame.sequence_number  // Use front frame's seq as aggregate seq
                        );
                    }
                    
                    // Update performance monitor
                    if (performance_monitor_) {
                        std::unordered_map<std::string, FrameData> frame_data_by_device = {
                            {front_frame.device_name, {front_frame.timestamp_us, front_frame.sequence_number, latency_front}},
                            {right_frame.device_name, {right_frame.timestamp_us, right_frame.sequence_number, latency_right}}
                        };
                        performance_monitor_->tick(frame_data_by_device);
                    }
                } else {
                    std::cout << "SYNC: No matching right frame for front ts=" << front_frame.timestamp_us << std::endl;
                }
            }
        }
        
        std::this_thread::sleep_for(poll_interval);
    }
}

bool Recorder::start_pipeline(
    CameraPipeline& pipeline,
    const std::string& device_name,
    std::unordered_map<std::string, int> cam_config,
    SinkMode mode,
    FrameCallback callback,
    bool trigger_record_flag,
    bool enable_fps_debug
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

bool Recorder::run(SinkMode mode) {
    // Set up signal handler for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize output files with timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream timestamp;
    timestamp << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    
    std::string output_subdir = output_dir_ + "/recording_" + timestamp.str();
    std::string front_video_path = output_subdir + "/cam_front.mp4";
    std::string right_video_path = output_subdir + "/cam_right.mp4";
    std::string sync_log_path = output_subdir + "/sync_log.jsonl";
    std::string metadata_path = output_subdir + "/metadata.json";
    
    // Create output directory
    system(("mkdir -p " + output_subdir).c_str());
    
    // Initialize video writers and sync logger
    int fps = CAM_CONFIG["/dev/cam_front"]["frame_rate"];
    int width = CAM_CONFIG["/dev/cam_front"]["width"];
    int height = CAM_CONFIG["/dev/cam_front"]["height"];
    
    if (!front_video_writer_->initialize(front_video_path, width, height, fps) ||
        !right_video_writer_->initialize(right_video_path, width, height, fps) ||
        !sync_logger_->initialize(sync_log_path) ||
        !performance_monitor_->initialize(output_subdir)) {
        std::cerr << "Failed to initialize output files" << std::endl;
        return false;
    }
    
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
    sync_thread_ = std::make_unique<std::thread>(&Recorder::sync_thread_func, this);
    
    // Keep the program running
    while (keep_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "\nShutting down..." << std::endl;
    
    // Wait for sync thread to finish
    if (sync_thread_ && sync_thread_->joinable()) {
        sync_thread_->join();
    }
    
    pipeline_front.stop();
    pipeline_right.stop();
    
    // Finalize output files
    front_video_writer_->finalize();
    right_video_writer_->finalize();
    sync_logger_->finalize();
    
    // Generate performance report
    if (performance_monitor_) {
        performance_monitor_->report();
    }
    
    // Write metadata file
    MetadataWriter::write_metadata(
        metadata_path,
        CAM_CONFIG,
        sync_tolerance_us_,
        front_video_path,
        right_video_path,
        sync_log_path
    );
    
    std::cout << "Recording saved to: " << output_subdir << std::endl;
    return true;
}
