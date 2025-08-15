#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <memory>
#include "camera_capture_pipeline.hpp"

class VideoWriter {
private:
    std::unique_ptr<cv::VideoWriter> writer_;
    std::string output_path_;
    bool is_initialized_;
    
public:
    VideoWriter();
    
    bool initialize(const std::string& path, int width, int height, double fps, const std::string& codec = "mp4v");
    /*
        Writes `frame` to video. Stores capture->disk latency in `latency_us`.
        Returns if the write is successful.
    */
    bool write_frame(const CameraFrame& frame, int& latency_us);
    void finalize();
    
    ~VideoWriter();
};
