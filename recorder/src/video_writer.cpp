#include "video_writer.hpp"

VideoWriter::VideoWriter() : is_initialized_(false) {}

bool VideoWriter::initialize(const std::string& path, int width, int height, double fps, const std::string& codec) {
    output_path_ = path;
    
    // Create VideoWriter with specified codec
    int fourcc = cv::VideoWriter::fourcc(codec[0], codec[1], codec[2], codec[3]);
    writer_ = std::make_unique<cv::VideoWriter>(path, fourcc, fps, cv::Size(width, height));
    
    if (!writer_->isOpened()) {
        std::cerr << "Failed to initialize VideoWriter for " << path << std::endl;
        return false;
    }
    
    is_initialized_ = true;
    std::cout << "VideoWriter initialized: " << path << " (" << width << "x" << height << " @ " << fps << "fps)" << std::endl;
    return true;
}

bool VideoWriter::write_frame(const CameraFrame& frame) {
    if (!is_initialized_ || !writer_) {
        std::cerr << "VideoWriter not initialized" << std::endl;
        return false;
    }
    
    // Convert based on explicit format
    cv::Mat bgr_image;
    
    switch (frame.format) {
        case CameraFormat::YUYV: {
            int expected_size = frame.width * frame.height * 2;
            
            // Create YUYV Mat - OpenCV expects this as CV_8UC2
            cv::Mat yuyv_image(frame.height, frame.width, CV_8UC2);
            
            // Copy YUYV data
            size_t bytes_to_copy = std::min(frame.image_data.size(), (size_t)expected_size);
            std::memcpy(yuyv_image.data, frame.image_data.data(), bytes_to_copy);
            
            // Convert YUYV to BGR using OpenCV
            cv::cvtColor(yuyv_image, bgr_image, cv::COLOR_YUV2BGR_YUY2);
            break;
        }
        
        case CameraFormat::RGB: {
            int expected_size = frame.width * frame.height * 3;
            
            cv::Mat rgb_image(frame.height, frame.width, CV_8UC3);
            size_t bytes_to_copy = std::min(frame.image_data.size(), (size_t)expected_size);
            std::memcpy(rgb_image.data, frame.image_data.data(), bytes_to_copy);
            
            cv::cvtColor(rgb_image, bgr_image, cv::COLOR_RGB2BGR);
            break;
        }
        
        case CameraFormat::GRAY: {
            int expected_size = frame.width * frame.height;
            
            cv::Mat gray_image(frame.height, frame.width, CV_8UC1);
            size_t bytes_to_copy = std::min(frame.image_data.size(), (size_t)expected_size);
            std::memcpy(gray_image.data, frame.image_data.data(), bytes_to_copy);
            
            cv::cvtColor(gray_image, bgr_image, cv::COLOR_GRAY2BGR);
            break;
        }
        
        default:
            std::cerr << "  Unsupported camera format!" << std::endl;
            return false;
    }
    
    writer_->write(bgr_image);
    return true;
}

void VideoWriter::finalize() {
    if (writer_) {
        writer_->release();
        writer_.reset();
    }
    is_initialized_ = false;
    std::cout << "VideoWriter finalized: " << output_path_ << std::endl;
}

VideoWriter::~VideoWriter() {
    finalize();
}
