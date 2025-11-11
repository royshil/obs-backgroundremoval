cmake_minimum_required(VERSION 3.16)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

set(VCPKG_TARGET_TRIPLET "" CACHE STRING "Vcpkg target triplet to use")
option(USE_PKGCONFIG "Use pkg-config to find dependencies" ON)
option(USE_SYSTEM_ONNXRUNTIME "Use system ONNX Runtime" ON)

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

if(APPLE)
  list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/onnxruntime")
  find_package(onnxruntime CONFIG REQUIRED)
  get_target_property(ONNXRUNTIME_DYLIB_PATH onnxruntime::onnxruntime IMPORTED_LOCATION_RELEASE)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE onnxruntime::onnxruntime)

  if(NOT USE_SYSTEM_ONNXRUNTIME)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE "${ONNXRUNTIME_DYLIB_PATH}")
    set_property(SOURCE "${ONNXRUNTIME_DYLIB_PATH}" PROPERTY MACOSX_PACKAGE_LOCATION Frameworks)
    set_target_properties(
      ${CMAKE_PROJECT_NAME}
      PROPERTIES XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "@loader_path/../Frameworks"
    )
  endif()
elseif(MSVC)
  if(USE_SYSTEM_ONNXRUNTIME)
    message(FATAL_ERROR "USE_SYSTEM_ONNXRUNTIME is not supported on Windows")
  endif()

  set(ONNXRUNTIME_DIR "${CMAKE_SOURCE_DIR}/onnxruntime")

  add_library(onnxruntime::onnxruntime_provider_shared SHARED IMPORTED)
  set_target_properties(
    onnxruntime::onnxruntime_provider_shared
    PROPERTIES
      IMPORTED_IMPLIB "${ONNXRUNTIME_DIR}/lib/onnxruntime_providers_shared.lib"
      IMPORTED_LOCATION "${ONNXRUNTIME_DIR}/lib/onnxruntime_providers_shared.dll"
      INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_DIR}/include"
  )

  add_library(onnxruntime::onnxruntime SHARED IMPORTED)
  set_target_properties(
    onnxruntime::onnxruntime
    PROPERTIES
      IMPORTED_IMPLIB "${ONNXRUNTIME_DIR}/lib/onnxruntime.lib"
      IMPORTED_LOCATION "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll"
      INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_DIR}/include"
      INTERFACE_LINK_OPTIONS "/DELAYLOAD:onnxruntime.dll"
  )
  target_link_libraries(onnxruntime::onnxruntime INTERFACE onnxruntime::onnxruntime_provider_shared)

  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE onnxruntime::onnxruntime)

  target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/DelayLoad.cpp)
  install(
    FILES
      "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll"
      "${ONNXRUNTIME_DIR}/lib/onnxruntime.pdb"
      "${ONNXRUNTIME_DIR}/lib/onnxruntime_providers_shared.dll"
      "${ONNXRUNTIME_DIR}/lib/onnxruntime_providers_shared.pdb"
    DESTINATION "${CMAKE_PROJECT_NAME}/bin/64bit/obs-backgroundremoval"
  )
else()
  if(NOT USE_SYSTEM_ONNXRUNTIME)
    set(ONNXRUNTIME_DIR "${CMAKE_SOURCE_DIR}/onnxruntime")
    list(PREPEND CMAKE_PREFIX_PATH "${ONNXRUNTIME_DIR}")
  endif()

  find_package(onnxruntime CONFIG REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE onnxruntime::onnxruntime)

  if(NOT USE_SYSTEM_ONNXRUNTIME)
    file(GLOB ONNXRUNTIME_SO_FILES "${ONNXRUNTIME_DIR}/lib64/libonnxruntime.so.*")
    install(
      FILES ${ONNXRUNTIME_SO_FILES} "${ONNXRUNTIME_DIR}/lib64/libonnxruntime_providers_shared.so"
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/obs-plugins/${CMAKE_PROJECT_NAME}
    )
    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/${CMAKE_PROJECT_NAME}")
  endif()
endif()

add_subdirectory(src/update-checker/CurlClient)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE CurlClient OpenCV::opencv_core OpenCV::opencv_imgproc)

# Check for ONNX Runtime Execution Providers
include(CheckLibraryExists)

check_library_exists(
  onnxruntime::onnxruntime
  OrtSessionOptionsAppendExecutionProvider_CUDA
  onnxruntime_c_api.h
  HAVE_ONNXRUNTIME_CUDA_EP
)
if(HAVE_ONNXRUNTIME_CUDA_EP)
  message(STATUS "ONNX Runtime CUDA Execution Provider found")
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE HAVE_ONNXRUNTIME_CUDA_EP)
endif()

check_library_exists(
  onnxruntime::onnxruntime
  OrtSessionOptionsAppendExecutionProvider_ROCM
  onnxruntime_c_api.h
  HAVE_ONNXRUNTIME_ROCM_EP
)
if(HAVE_ONNXRUNTIME_ROCM_EP)
  message(STATUS "ONNX Runtime ROCM Execution Provider found")
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE HAVE_ONNXRUNTIME_ROCM_EP)
endif()

check_library_exists(
  onnxruntime::onnxruntime
  OrtSessionOptionsAppendExecutionProvider_Tensorrt
  onnxruntime_c_api.h
  HAVE_ONNXRUNTIME_TENSORRT_EP
)
if(HAVE_ONNXRUNTIME_TENSORRT_EP)
  message(STATUS "ONNX Runtime TensorRT Execution Provider found")
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE HAVE_ONNXRUNTIME_TENSORRT_EP)
endif()

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
