cmake_minimum_required(VERSION 3.16)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

set(VCPKG_TARGET_TRIPLET "" CACHE STRING "Vcpkg target triplet to use")
option(USE_PKGCONFIG "Use pkg-config to find dependencies" OFF)

option(USE_SYSTEM_ONNXRUNTIME "Use system ONNX Runtime" OFF)
option(DISABLE_ONNXRUNTIME_GPU "Disables GPU support of ONNX Runtime (Only valid on Linux)" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

if(NOT VCPKG_TARGET_TRIPLET STREQUAL "")
  # Use vcpkg to find dependencies (Official binary)

  message(STATUS "Using vcpkg with target triplet: ${VCPKG_TARGET_TRIPLET}")

  list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}")

  find_package(CURL CONFIG REQUIRED)
  find_package(OpenCV CONFIG REQUIRED)

  add_library(OpenCV::opencv_core ALIAS opencv_core)
  add_library(OpenCV::opencv_imgproc ALIAS opencv_imgproc)
elseif(USE_PKGCONFIG)
  # Use pkg-config to find dependencies (Arch Linux, etc.)

  message(STATUS "Using pkg-config to find dependencies")

  find_package(PkgConfig REQUIRED)

  # CURL
  pkg_check_modules(PC_CURL REQUIRED libcurl)
  add_library(CURL::libcurl INTERFACE IMPORTED)
  target_link_libraries(CURL::libcurl INTERFACE ${PC_CURL_LIBRARIES})
  target_include_directories(CURL::libcurl INTERFACE ${PC_CURL_INCLUDE_DIRS})
  target_compile_definitions(CURL::libcurl INTERFACE ${PC_CURL_CFLAGS_OTHER})
  target_link_directories(CURL::libcurl INTERFACE ${PC_CURL_LIBRARY_DIRS})

  # OpenCV
  pkg_check_modules(PC_OPENCV REQUIRED opencv4)
  add_library(OpenCV::opencv_core INTERFACE IMPORTED)
  target_link_libraries(OpenCV::opencv_core INTERFACE opencv_core)
  target_include_directories(OpenCV::opencv_core INTERFACE ${PC_OPENCV_INCLUDE_DIRS})
  target_compile_definitions(OpenCV::opencv_core INTERFACE ${PC_OPENCV_CFLAGS_OTHER})
  target_link_directories(OpenCV::opencv_core INTERFACE ${PC_OPENCV_LIBRARY_DIRS})
  add_library(OpenCV::opencv_imgproc INTERFACE IMPORTED)
  target_link_libraries(OpenCV::opencv_imgproc INTERFACE opencv_imgproc)
  target_link_directories(OpenCV::opencv_imgproc INTERFACE ${PC_OPENCV_LIBRARY_DIRS})
else()
  # Let system find dependencies (Fallback)

  message(STATUS "Using system to find dependencies")

  find_package(CURL REQUIRED)
  find_package(OpenCV CONFIG REQUIRED)
endif()

if(DISABLE_ONNXRUNTIME_GPU)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE DISABLE_ONNXRUNTIME_GPU)
endif()

if(USE_SYSTEM_ONNXRUNTIME)
  if(OS_LINUX)
    find_package(Onnxruntime 1.16.3 REQUIRED)
    set(
      Onnxruntime_INCLUDE_PATH
      ${Onnxruntime_INCLUDE_DIR}
      ${Onnxruntime_INCLUDE_DIR}/onnxruntime
      ${Onnxruntime_INCLUDE_DIR}/onnxruntime/core/session
      ${Onnxruntime_INCLUDE_DIR}/onnxruntime/core/providers/cpu
    )
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE "${Onnxruntime_LIBRARIES}")
    target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM PUBLIC "${Onnxruntime_INCLUDE_PATH}")
  else()
    message(FATAL_ERROR "System ONNX Runtime is only supported on Linux!")
  endif()
else()
  include(cmake/FetchOnnxruntime.cmake)
endif()

add_subdirectory(src/update-checker/CurlClient)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE CurlClient OpenCV::opencv_core OpenCV::opencv_imgproc)

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.c
    src/ort-utils/ort-session-utils.cpp
    src/obs-utils/obs-utils.cpp
    src/obs-utils/obs-config-utils.cpp
    src/update-checker/github-utils.cpp
    src/update-checker/update-checker.cpp
    src/background-filter-info.c
    src/background-filter.cpp
    src/enhance-filter.cpp
    src/enhance-filter-info.c
)
target_compile_options(
  ${CMAKE_PROJECT_NAME}
  PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-error=unused-command-line-argument>
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
