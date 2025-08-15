cmake_minimum_required(VERSION 3.15)
project(robodaq-recorder LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)
endif(COMMAND cmake_policy)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0)
pkg_check_modules(GST_APP REQUIRED gstreamer-app-1.0)
find_package(OpenCV REQUIRED)

add_executable(recorder
    src/camera_capture_pipeline.cpp
    src/camera_capture_pipeline.hpp
    src/main.cpp
    src/metadata_writer.cpp
    src/metadata_writer.hpp
    src/mpmc_ring_buffer.hpp
    src/performance_monitor.cpp
    src/performance_monitor.hpp
    src/recorder.cpp
    src/recorder.hpp
    src/spsc_ring_buffer.hpp
    src/sync_logger.cpp
    src/sync_logger.hpp
    src/video_writer.cpp
    src/video_writer.hpp
)

target_include_directories(recorder PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GST_INCLUDE_DIRS}
    ${GST_APP_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(recorder 
    ${GST_LIBRARIES}
    ${GST_APP_LIBRARIES}
    ${OpenCV_LIBS}
)

target_compile_options(recorder PRIVATE ${GST_CFLAGS_OTHER} ${GST_APP_CFLAGS_OTHER})
