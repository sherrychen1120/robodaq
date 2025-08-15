find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0)

add_executable(recorder
    src/main.cpp
    src/ring_buffer.hpp
    src/camera_capture_pipeline.hpp
)

target_include_directories(recorder PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GST_INCLUDE_DIRS}
)

target_link_libraries(recorder 
    ${GST_LIBRARIES}
)

target_compile_options(recorder PRIVATE ${GST_CFLAGS_OTHER})
