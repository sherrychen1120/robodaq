#ifndef CAMERA_CAPTURE_PIPELINE_H
#define CAMERA_CAPTURE_PIPELINE_H

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

// Camera frame structure containing all frame data
struct CameraFrame {
    uint64_t sequence_number;
    uint64_t timestamp_us;  // Timestamp in microseconds
    std::string device_name;
    std::vector<uint8_t> image_data;
    int width;
    int height;
    int channels;  // Number of color channels (e.g., 3 for RGB)
    
    CameraFrame() : sequence_number(0), timestamp_us(0), width(0), height(0), channels(0) {}
};

// Callback type for frame processing
using FrameCallback = std::function<void(const CameraFrame& frame, bool trigger_record)>;

class CameraPipeline {
private:
    GstElement *pipeline, *source, *capsfilter, *queue, *sink;
    bool gst_initialized;
    SinkMode sink_mode;
    FrameCallback frame_callback;
    std::string device_name;
    bool trigger_record;
    uint64_t sequence_counter;
    
    // Static callback function for appsink
    static GstFlowReturn on_new_sample(GstAppSink* appsink, gpointer user_data) {
        CameraPipeline* pipeline = static_cast<CameraPipeline*>(user_data);
        
        // Pull the sample
        GstSample* sample = gst_app_sink_pull_sample(appsink);
        if (!sample) {
            return GST_FLOW_ERROR;
        }
        
        // Get buffer and caps
        GstBuffer* buffer = gst_sample_get_buffer(sample);
        GstCaps* caps = gst_sample_get_caps(sample);
        
        if (!buffer || !caps) {
            gst_sample_unref(sample);
            return GST_FLOW_ERROR;
        }
        
        // Extract frame information
        GstStructure* structure = gst_caps_get_structure(caps, 0);
        int width, height;
        gst_structure_get_int(structure, "width", &width);
        gst_structure_get_int(structure, "height", &height);
        
        // Map buffer to access data
        GstMapInfo map;
        if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            gst_sample_unref(sample);
            return GST_FLOW_ERROR;
        }
        
        // Create CameraFrame
        CameraFrame frame;
        frame.sequence_number = ++pipeline->sequence_counter;
        frame.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        frame.device_name = pipeline->device_name;
        frame.width = width;
        frame.height = height;
        frame.channels = 3; // Assuming RGB
        
        // Copy image data
        frame.image_data.resize(map.size);
        std::memcpy(frame.image_data.data(), map.data, map.size);
        
        // Unmap buffer and clean up
        gst_buffer_unmap(buffer, &map);
        gst_sample_unref(sample);
        
        // Call the user callback if set
        if (pipeline->frame_callback) {
            pipeline->frame_callback(frame, pipeline->trigger_record);
        }
        
        return GST_FLOW_OK;
    }

public:
    CameraPipeline() 
    : pipeline(nullptr), source(nullptr), capsfilter(nullptr), 
      queue(nullptr), sink(nullptr), gst_initialized(false), 
      sink_mode(SinkMode::DISPLAY), trigger_record(false), sequence_counter(0) {}
    
    bool initialize(
        const std::string& device, 
        int width, 
        int height,
        int framerate,
        SinkMode mode = SinkMode::DISPLAY,
        FrameCallback callback = nullptr,
        bool trigger_record_flag = false,
        bool enable_fps_debug = false
) {
        // Store configuration
        sink_mode = mode;
        frame_callback = callback;
        device_name = device;
        trigger_record = trigger_record_flag;
        
        // Initialize GStreamer
        if (!gst_initialized) {
            gst_init(nullptr, nullptr);
            gst_initialized = true;
            
            // Enable FPS debug logging if requested
            if (enable_fps_debug && mode == SinkMode::DISPLAY) {
                gst_debug_set_threshold_for_name("fpsdisplaysink", GST_LEVEL_LOG);
            }
        }
        
        pipeline = gst_pipeline_new("camera-pipeline");
        source = gst_element_factory_make("v4l2src", "camera-source");
        capsfilter = gst_element_factory_make("capsfilter", "caps-filter");
        queue = gst_element_factory_make("queue", "ring-buffer");
        
        // Create appropriate sink based on mode
        if (mode == SinkMode::DISPLAY) {
            sink = gst_element_factory_make("fpsdisplaysink", "fps-sink");
        } else {
            sink = gst_element_factory_make("appsink", "app-sink");
        }
        
        // Check if elements were created successfully
        if (!pipeline || !source || !capsfilter || !queue || !sink) {
            g_printerr("Failed to create GStreamer elements\n");
            return false;
        }
        
        // Create caps for video format
        GstCaps *caps = gst_caps_new_simple("video/x-raw",
            "width", G_TYPE_INT, width,
            "height", G_TYPE_INT, height,
            "framerate", GST_TYPE_FRACTION, framerate, 1,
            nullptr);
        g_object_set(capsfilter, "caps", caps, nullptr);
        gst_caps_unref(caps);
        
        // Set properties
        g_object_set(source, "device", device.c_str(), nullptr);
        g_object_set(queue, "max-size-buffers", 30, nullptr);
        g_object_set(queue, "leaky", 2, nullptr); // downstream
        
        // Configure sink based on mode
        if (mode == SinkMode::DISPLAY) {
            g_object_set(sink, "sync", FALSE, nullptr);
            g_object_set(sink, "text-overlay", TRUE, nullptr);  // Show FPS overlay on video
            g_object_set(sink, "fps-update-interval", 100, nullptr);  // Update FPS every 100ms
        } else {
            // Configure appsink
            g_object_set(sink, "emit-signals", TRUE, nullptr);
            g_object_set(sink, "sync", FALSE, nullptr);
            g_object_set(sink, "max-buffers", 1, nullptr);  // Keep only latest frame
            g_object_set(sink, "drop", TRUE, nullptr);      // Drop old frames if not consumed
            
            // Set up the callback
            GstAppSinkCallbacks callbacks = {0};
            callbacks.new_sample = on_new_sample;
            gst_app_sink_set_callbacks(GST_APP_SINK(sink), &callbacks, this, nullptr);
        }
        
        // Add elements and link
        gst_bin_add_many(GST_BIN(pipeline), source, capsfilter, queue, sink, nullptr);
        if (!gst_element_link_many(source, capsfilter, queue, sink, nullptr)) {
            g_printerr("Failed to link GStreamer elements\n");
            return false;
        }
        
        return true;
    }
    
    bool start() {
        if (!pipeline) {
            g_printerr("Pipeline not initialized\n");
            return false;
        }
        
        GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            g_printerr("Failed to start pipeline\n");
            return false;
        }
        
        return true;
    }
    
    void stop() {
        if (pipeline) {
            gst_element_set_state(pipeline, GST_STATE_NULL);
        }
    }
    
    ~CameraPipeline() {
        stop();
        if (pipeline) {
            gst_object_unref(pipeline);
        }
    }
};

#endif // CAMERA_CAPTURE_PIPELINE_H
