#include "camera_capture_pipeline.hpp"

// CameraPipeline constructor
CameraPipeline::CameraPipeline() 
    : pipeline_(nullptr), source_(nullptr), capsfilter_(nullptr), 
      queue_(nullptr), sink_(nullptr), gst_initialized_(false), 
      sink_mode_(SinkMode::DISPLAY), trigger_record_(false), sequence_counter_(0),
      camera_format_(CAMERA_CAPTURE_FORMAT) {}

// CameraPipeline destructor
CameraPipeline::~CameraPipeline() {
    stop();
    if (pipeline_) {
        gst_object_unref(pipeline_);
    }
}

// Static callback function for appsink
GstFlowReturn CameraPipeline::on_new_sample_(GstAppSink* appsink, gpointer user_data) {
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
    
    // Get format information for debugging
    const gchar* format_str = gst_structure_get_string(structure, "format");
    
    // Map buffer to access data
    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }
    
    // Create CameraFrame
    CameraFrame frame;
    frame.sequence_number = ++pipeline->sequence_counter_;
    frame.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    frame.device_name = pipeline->device_name_;
    frame.width = width;
    frame.height = height;
    frame.format = pipeline->camera_format_;  // Set explicit format
    
    // Log format information
    const char* format_name;
    switch (frame.format) {
        case CameraFormat::YUYV: format_name = "YUYV"; break;
        case CameraFormat::RGB: format_name = "RGB"; break;
        case CameraFormat::GRAY: format_name = "GRAY"; break;
    }
    
    // Copy image data
    frame.image_data.resize(map.size);
    std::memcpy(frame.image_data.data(), map.data, map.size);
    
    // Unmap buffer and clean up
    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);
    
    // Call the user callback if set
    if (pipeline->frame_callback_) {
        pipeline->frame_callback_(frame, pipeline->trigger_record_);
    }
    
    return GST_FLOW_OK;
}

bool CameraPipeline::initialize(
    const std::string& device, 
    int width, 
    int height,
    int framerate,
    SinkMode mode,
    FrameCallback callback,
    bool trigger_record_flag,
    bool enable_fps_debug
) {
    // Store configuration
    sink_mode_ = mode;
    frame_callback_ = callback;
    device_name_ = device;
    trigger_record_ = trigger_record_flag;
    
    // Initialize GStreamer
    if (!gst_initialized_) {
        gst_init(nullptr, nullptr);
        gst_initialized_ = true;
        
        // Enable FPS debug logging if requested
        if (enable_fps_debug && mode == SinkMode::DISPLAY) {
            gst_debug_set_threshold_for_name("fpsdisplaysink", GST_LEVEL_LOG);
        }
    }
    
    pipeline_ = gst_pipeline_new("camera-pipeline");
    source_ = gst_element_factory_make("v4l2src", "camera-source");
    capsfilter_ = gst_element_factory_make("capsfilter", "caps-filter");
    queue_ = gst_element_factory_make("queue", "ring-buffer");
    
    // Create appropriate sink based on mode
    if (mode == SinkMode::DISPLAY) {
        sink_ = gst_element_factory_make("fpsdisplaysink", "fps-sink");
    } else {
        sink_ = gst_element_factory_make("appsink", "app-sink");
    }
    
    // Check if elements were created successfully
    if (!pipeline_ || !source_ || !capsfilter_ || !queue_ || !sink_) {
        g_printerr("Failed to create GStreamer elements\n");
        return false;
    }
    
    // Create caps for video format based on configured format
    const char* gst_format_string;
    switch (camera_format_) {
        case CameraFormat::YUYV:
            gst_format_string = "YUY2";  // GStreamer name for YUYV
            break;
        case CameraFormat::RGB:
            gst_format_string = "RGB";
            break;
        case CameraFormat::GRAY:
            gst_format_string = "GRAY8";
            break;
    }
    
    GstCaps *caps = gst_caps_new_simple("video/x-raw",
        "format", G_TYPE_STRING, gst_format_string,
        "width", G_TYPE_INT, width,
        "height", G_TYPE_INT, height,
        "framerate", GST_TYPE_FRACTION, framerate, 1,
        nullptr);
    g_object_set(capsfilter_, "caps", caps, nullptr);
    gst_caps_unref(caps);
    
    // Set properties
    g_object_set(source_, "device", device.c_str(), nullptr);
    g_object_set(queue_, "max-size-buffers", 30, nullptr);
    g_object_set(queue_, "leaky", 2, nullptr); // downstream
    
    // Configure sink based on mode
    if (mode == SinkMode::DISPLAY) {
        g_object_set(sink_, "sync", FALSE, nullptr);
        g_object_set(sink_, "text-overlay", TRUE, nullptr);  // Show FPS overlay on video
        g_object_set(sink_, "fps-update-interval", 100, nullptr);  // Update FPS every 100ms
    } else {
        // Configure appsink
        g_object_set(sink_, "emit-signals", TRUE, nullptr);
        g_object_set(sink_, "sync", FALSE, nullptr);
        g_object_set(sink_, "max-buffers", 1, nullptr);  // Keep only latest frame
        g_object_set(sink_, "drop", TRUE, nullptr);      // Drop old frames if not consumed
        
        // Set up the callback
        GstAppSinkCallbacks callbacks = {0};
        callbacks.new_sample = on_new_sample_;
        gst_app_sink_set_callbacks(GST_APP_SINK(sink_), &callbacks, this, nullptr);
    }
    
    // Add elements and link
    gst_bin_add_many(GST_BIN(pipeline_), source_, capsfilter_, queue_, sink_, nullptr);
    if (!gst_element_link_many(source_, capsfilter_, queue_, sink_, nullptr)) {
        g_printerr("Failed to link GStreamer elements\n");
        return false;
    }
    
    return true;
}

bool CameraPipeline::start() {
    if (!pipeline_) {
        g_printerr("Pipeline not initialized\n");
        return false;
    }
    
    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Failed to start pipeline\n");
        return false;
    }
    
    return true;
}

void CameraPipeline::stop() {
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
    }
}
