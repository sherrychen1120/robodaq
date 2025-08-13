#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <thread>
#include <filesystem>

const int TARGET_FPS = 30;
const int CAPTURE_LOOP_DURATION_SECONDS = 10;

/*
    Keeps track of the timing data and record each sample to a file.
*/
class TimeLogger {
    
    private:
        std::string log_file;
        std::ofstream log_fstream;
        std::string device_name;
        int sequence_number;
    
    public:
        TimeLogger(
            const std::string& log_file, 
            const std::string& device_name
        ) : log_file(log_file), device_name(device_name), sequence_number(0) {
            // Check if file already exists
            if (std::filesystem::exists(log_file)) {
                throw std::runtime_error("File already exists: " + log_file);
            }
            
            log_fstream.open(log_file, std::ios::out);
            if (!log_fstream.is_open()) {
                throw std::runtime_error("Failed to open log file: " + log_file);
            }
        }
        
        ~TimeLogger() {
            if (log_fstream.is_open()) {
                log_fstream.close();
            }
        }
        void record_timing_data(
            const std::chrono::steady_clock::time_point& expected_time
        ) {
            // wall time
            auto ts_wall_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();

            // monotonic time
            auto ts_mono_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count();
            auto target_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                expected_time.time_since_epoch()
            ).count();
            auto jitter_ns = ts_mono_ns - target_time_ns;

            log_fstream << "{"
                        << "\"device\": \"" << device_name << "\", "
                        << "\"sequence_number\": " << sequence_number << ", "
                        << "\"ts_mono_ns\": " << ts_mono_ns << ", "
                        << "\"ts_wall_ns\": " << ts_wall_ns << ", "
                        << "\"ts_target_ns\": " << target_time_ns << ", "
                        << "\"jitter_ns\": " << jitter_ns 
                        << "}" << std::endl;
            
            sequence_number++;

        }

};

/*
    Emulates a device (camera / robot) that ticks at a target FPS.
    Uses a TimeLogger to record timing data.
*/
class TestDevice {
    private:
        // Must be declared before time_logger
        std::string device_name = "test_device";
        TimeLogger time_logger;
        int target_fps;
        std::chrono::nanoseconds target_period;
        std::chrono::steady_clock::time_point last_expected_time;
    
    public:
        TestDevice(
            const std::string& log_file, 
            const int target_fps
        ) : time_logger(log_file, device_name), target_fps(target_fps) {
            target_period = std::chrono::nanoseconds(static_cast<long long>(1e9 / target_fps));
            last_expected_time = std::chrono::steady_clock::now();
        }

        void tick() {
            time_logger.record_timing_data(last_expected_time);

            // Do work here

            // Update expected time.
            last_expected_time += target_period;
            if (std::chrono::steady_clock::now() < last_expected_time) {
                std::this_thread::sleep_until(last_expected_time);
            }
        }
};


int main(int argc, char* argv[]) {
    try {
        // Check if output file argument is provided
        if (argc < 2) {
            std::cerr << "Usage: " << argv[0] << " <output_file.jsonl>" << std::endl;
            return 1;
        }
        std::string output_file = argv[1];
        
        TestDevice test_device(output_file, TARGET_FPS);

        std::cout << "Running " << TARGET_FPS << " Hz capture loop for 10 seconds..." << std::endl;
        auto start_time = std::chrono::steady_clock::now();
        auto end_time = start_time + std::chrono::seconds(CAPTURE_LOOP_DURATION_SECONDS);
        while (std::chrono::steady_clock::now() < end_time) {
            test_device.tick();
        }
        
        std::cout << "Capture loop timing is logged to " << output_file << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}