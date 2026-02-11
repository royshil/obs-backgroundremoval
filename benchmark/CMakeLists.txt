cmake_minimum_required(VERSION 3.16)
project(obs-bgremoval-bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenCV REQUIRED COMPONENTS core)
find_package(Threads REQUIRED)
find_package(onnxruntime CONFIG QUIET)

add_executable(bench bench.cpp)
target_include_directories(bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(bench PRIVATE ${OpenCV_LIBS} Threads::Threads)

if(onnxruntime_FOUND)
  message(STATUS "ONNX Runtime found - CPU utilization test enabled")
  target_compile_definitions(bench PRIVATE HAS_ONNXRUNTIME=1)
  target_link_libraries(bench PRIVATE onnxruntime::onnxruntime)
else()
  message(STATUS "ONNX Runtime not found - CPU utilization test disabled")
endif()
