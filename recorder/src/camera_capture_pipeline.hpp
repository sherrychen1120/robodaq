#pragma once

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <string>
#include <functional>
#include <iostream>
#include <chrono>
#include <vector>
#include <cstdint>
#include <cstring>

enum class SinkMode {
    DISPLAY,  // Use fpsdisplaysink for visual output
    APPSINK   // Use appsink for programmatic frame access
};

enum class CameraFormat {
    YUYV,     // YUV 4:2:2 packed format (native to most USB cameras)
    RGB,      // RGB format
    GRAY      // Grayscale
};

// Camera capture configuration
constexpr CameraFormat CAMERA_CAPTURE_FORMAT = CameraFormat::YUYV;

// Camera frame structure containing all frame data
struct CameraFrame {
    uint64_t sequence_number;
    uint64_t timestamp_us;  // Timestamp in microseconds
    std::string device_name;
    std::vector<uint8_t> image_data;
    int width;
    int height;
    CameraFormat format;

    CameraFrame() 
    : sequence_number(0), timestamp_us(0), 
      width(0), height(0), format(CameraFormat::YUYV) {}
};

// Callback type for frame processing
using FrameCallback = std::function<void(const CameraFrame& frame, bool trigger_record)>;

class CameraPipeline {
private:
    GstElement *pipeline_, *source_, *capsfilter_, *queue_, *sink_;
    bool gst_initialized_;
    SinkMode sink_mode_;
    FrameCallback frame_callback_;
    std::string device_name_;
    bool trigger_record_;
    uint64_t sequence_counter_;
    CameraFormat camera_format_;
    
    // Static callback function for appsink
    static GstFlowReturn on_new_sample_(GstAppSink* appsink, gpointer user_data);

public:
    CameraPipeline();
    ~CameraPipeline();
    
    bool initialize(
        const std::string& device, 
        int width, 
        int height,
        int framerate,
        SinkMode mode = SinkMode::DISPLAY,
        FrameCallback callback = nullptr,
        bool trigger_record_flag = false,
        bool enable_fps_debug = false
    );
    
    bool start();
    void stop();
};
