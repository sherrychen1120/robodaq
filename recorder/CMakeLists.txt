add_executable(recorder
    src/main.cpp
    src/ring_buffer.hpp
)
target_include_directories(recorder PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
