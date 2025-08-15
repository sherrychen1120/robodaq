#ifndef CAMERA_CAPTURE_PIPELINE_H
#define CAMERA_CAPTURE_PIPELINE_H

#include <gst/gst.h>
#include <string>

class CameraPipeline {
private:
    GstElement *pipeline, *source, *capsfilter, *queue, *sink;
    bool gst_initialized;
    
public:
    CameraPipeline() 
    : pipeline(nullptr), source(nullptr), capsfilter(nullptr), 
      queue(nullptr), sink(nullptr), gst_initialized(false) {}
    
    bool initialize(
        const std::string& device, 
        int width, 
        int height,
        int framerate,
        bool enable_fps_debug = false
) {
        // Initialize GStreamer
        if (!gst_initialized) {
            gst_init(nullptr, nullptr);
            gst_initialized = true;
            
            // Enable FPS debug logging if requested
            if (enable_fps_debug) {
                gst_debug_set_threshold_for_name("fpsdisplaysink", GST_LEVEL_LOG);
            }
        }
        
        pipeline = gst_pipeline_new("camera-pipeline");
        source = gst_element_factory_make("v4l2src", "camera-source");
        capsfilter = gst_element_factory_make("capsfilter", "caps-filter");
        queue = gst_element_factory_make("queue", "ring-buffer");
        sink = gst_element_factory_make("fpsdisplaysink", "fps-sink");
        
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
        g_object_set(sink, "sync", FALSE, nullptr);
        g_object_set(sink, "text-overlay", TRUE, nullptr);  // Show FPS overlay on video
        g_object_set(sink, "fps-update-interval", 100, nullptr);  // Update FPS every 100ms
        
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
