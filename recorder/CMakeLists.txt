find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0)
pkg_check_modules(GST_APP REQUIRED gstreamer-app-1.0)

add_executable(recorder
    src/camera_capture_pipeline.hpp
    src/main.cpp
    src/mpmc_ring_buffer.hpp
    src/recorder.hpp
    src/spsc_ring_buffer.hpp
)

target_include_directories(recorder PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GST_INCLUDE_DIRS}
    ${GST_APP_INCLUDE_DIRS}
)

target_link_libraries(recorder 
    ${GST_LIBRARIES}
    ${GST_APP_LIBRARIES}
)

target_compile_options(recorder PRIVATE ${GST_CFLAGS_OTHER} ${GST_APP_CFLAGS_OTHER})
