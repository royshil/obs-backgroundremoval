include(FetchContent)

set(CUSTOM_ONNXRUNTIME_URL "" CACHE STRING "URL of a downloaded ONNX Runtime tarball")

set(CUSTOM_ONNXRUNTIME_HASH "" CACHE STRING "Hash of a downloaded ONNX Runtime tarball")

set(CUSTOM_ONNXRUNTIME_VERSION "" CACHE STRING "Major.Minor.Patch of ONNX Runtime tarball")

set(Onnxruntime_VERSION "1.17.1")

if(CUSTOM_ONNXRUNTIME_URL STREQUAL "")
  set(USE_PREDEFINED_ONNXRUNTIME ON)
else()
  if(CUSTOM_ONNXRUNTIME_HASH STREQUAL "")
    message(FATAL_ERROR "Both of CUSTOM_ONNXRUNTIME_URL and CUSTOM_ONNXRUNTIME_HASH must be present!")
  else()
    set(USE_PREDEFINED_ONNXRUNTIME OFF)
  endif()
  if(CUSTOM_ONNXRUNTIME_VERSION STREQUAL "")
    message(FATAL_ERROR "CUSTOM_ONNXRUNTIME_VERSION must be set!")
  else()
    set(Onnxruntime_VERSION ${CUSTOM_ONNXRUNTIME_VERSION})
  endif()
endif()

if(USE_PREDEFINED_ONNXRUNTIME)
  set(Onnxruntime_BASEURL "https://github.com/microsoft/onnxruntime/releases/download/v${Onnxruntime_VERSION}")
  set(Onnxruntime_WINDOWS_VERSION "v${Onnxruntime_VERSION}-1")
  set(
    Onnxruntime_WINDOWS_BASEURL
    "https://github.com/occ-ai/occ-ai-dep-onnxruntime-static-win/releases/download/${Onnxruntime_WINDOWS_VERSION}"
  )

  if(APPLE)
    set(Onnxruntime_URL "${Onnxruntime_BASEURL}/onnxruntime-osx-universal2-${Onnxruntime_VERSION}.tgz")
    set(Onnxruntime_HASH SHA256=9FA57FA6F202A373599377EF75064AE568FDA8DA838632B26A86024C7378D306)
  elseif(MSVC)
    if(CMAKE_VS_PLATFORM_NAME STREQUAL "ARM64")
      # Use official Microsoft ONNX Runtime for ARM64 (no static build available yet)
      set(Onnxruntime_URL "${Onnxruntime_BASEURL}/onnxruntime-win-arm64-${Onnxruntime_VERSION}.zip")
      set(Onnxruntime_HASH "")
    else()
      set(Onnxruntime_URL "${Onnxruntime_WINDOWS_BASEURL}/onnxruntime-windows-${Onnxruntime_WINDOWS_VERSION}-Release.zip")
      set(OOnnxruntime_HASH SHA256=39E63850D9762810161AE1B4DEAE5E3C02363521273E4B894A9D9707AB626C38)
    endif()
  else()
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
      set(Onnxruntime_URL "${Onnxruntime_BASEURL}/onnxruntime-linux-aarch64-${Onnxruntime_VERSION}.tgz")
      set(Onnxruntime_HASH SHA256=70B6F536BB7AB5961D128E9DBD192368AC1513BFFB74FE92F97AAC342FBD0AC1)
    else()
      set(Onnxruntime_URL "${Onnxruntime_BASEURL}/onnxruntime-linux-x64-gpu-${Onnxruntime_VERSION}.tgz")
      set(Onnxruntime_HASH SHA256=613C53745EA4960ED368F6B3AB673558BB8561C84A8FA781B4EA7FB4A4340BE4)
    endif()
  endif()
else()
  set(Onnxruntime_URL "${CUSTOM_ONNXRUNTIME_URL}")
  set(Onnxruntime_HASH "${CUSTOM_ONNXRUNTIME_HASH}")
endif()

FetchContent_Declare(onnxruntime URL ${Onnxruntime_URL} URL_HASH ${Onnxruntime_HASH})
FetchContent_MakeAvailable(onnxruntime)

if(APPLE)
  set(Onnxruntime_LIB "${onnxruntime_SOURCE_DIR}/lib/libonnxruntime.${Onnxruntime_VERSION}.dylib")
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE "${Onnxruntime_LIB}")
  target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM PUBLIC "${onnxruntime_SOURCE_DIR}/include")
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE "${Onnxruntime_LIB}")
  set_property(SOURCE "${Onnxruntime_LIB}" PROPERTY MACOSX_PACKAGE_LOCATION Frameworks)
  source_group("Frameworks" FILES "${Onnxruntime_LIB}")
  add_custom_command(
    TARGET "${CMAKE_PROJECT_NAME}"
    POST_BUILD
    COMMAND
      ${CMAKE_INSTALL_NAME_TOOL} -change "@rpath/libonnxruntime.${Onnxruntime_VERSION}.dylib"
      "@loader_path/../Frameworks/libonnxruntime.${Onnxruntime_VERSION}.dylib" $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
  )
elseif(MSVC)
  if(CMAKE_VS_PLATFORM_NAME STREQUAL "ARM64")
    # ARM64 uses dynamic ONNX Runtime (no static build with DirectML available yet)
    set(Onnxruntime_LIB "${onnxruntime_SOURCE_DIR}/lib/onnxruntime.dll")
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE "${onnxruntime_SOURCE_DIR}/lib/onnxruntime.lib")
    target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM PUBLIC "${onnxruntime_SOURCE_DIR}/include")
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE DISABLE_ONNXRUNTIME_GPU)
    
    # Copy DLL to output directory
    add_custom_command(
      TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${onnxruntime_SOURCE_DIR}/lib/onnxruntime.dll"
        "$<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>"
      COMMENT "Copying ONNX Runtime DLL for ARM64"
    )
    
    install(FILES "${onnxruntime_SOURCE_DIR}/lib/onnxruntime.dll" DESTINATION "obs-plugins/64bit")
  else()
    # x64 uses static ONNX Runtime with DirectML
    add_library(Ort INTERFACE)
  set(
    Onnxruntime_LIB_NAMES
    session;providers_shared;providers_dml;optimizer;providers;framework;graph;util;mlas;common;flatbuffers
  )
  foreach(lib_name IN LISTS Onnxruntime_LIB_NAMES)
    add_library(Ort::${lib_name} STATIC IMPORTED)
    set_target_properties(
      Ort::${lib_name}
      PROPERTIES IMPORTED_LOCATION ${onnxruntime_SOURCE_DIR}/lib/onnxruntime_${lib_name}.lib
    )
    set_target_properties(Ort::${lib_name} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${onnxruntime_SOURCE_DIR}/include)
    target_link_libraries(Ort INTERFACE Ort::${lib_name})
  endforeach()

  set(
    Onnxruntime_EXTERNAL_LIB_NAMES
    onnx;onnx_proto;libprotobuf-lite;re2;absl_throw_delegate;absl_hash;absl_city;absl_low_level_hash;absl_raw_hash_set
  )
  foreach(lib_name IN LISTS Onnxruntime_EXTERNAL_LIB_NAMES)
    add_library(Ort::${lib_name} STATIC IMPORTED)
    set_target_properties(Ort::${lib_name} PROPERTIES IMPORTED_LOCATION ${onnxruntime_SOURCE_DIR}/lib/${lib_name}.lib)
    target_link_libraries(Ort INTERFACE Ort::${lib_name})
  endforeach()

  add_library(Ort::DirectML SHARED IMPORTED)
  set_target_properties(Ort::DirectML PROPERTIES IMPORTED_LOCATION ${onnxruntime_SOURCE_DIR}/bin/DirectML.dll)
  set_target_properties(Ort::DirectML PROPERTIES IMPORTED_IMPLIB ${onnxruntime_SOURCE_DIR}/bin/DirectML.lib)

  target_link_libraries(
    Ort
    INTERFACE Ort::DirectML d3d12.lib dxgi.lib dxguid.lib Dxcore.lib
  )

  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Ort)

  install(IMPORTED_RUNTIME_ARTIFACTS Ort::DirectML DESTINATION "obs-plugins/64bit")
  endif()
else()
  if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(Onnxruntime_LINK_LIBS "${onnxruntime_SOURCE_DIR}/lib/libonnxruntime.so.${Onnxruntime_VERSION}")
    set(Onnxruntime_INSTALL_LIBS ${Onnxruntime_LINK_LIBS})
  else()
    set(Onnxruntime_LINK_LIBS "${onnxruntime_SOURCE_DIR}/lib/libonnxruntime.so.${Onnxruntime_VERSION}")
    set(
      Onnxruntime_INSTALL_LIBS
      ${Onnxruntime_LINK_LIBS}
      "${onnxruntime_SOURCE_DIR}/lib/libonnxruntime_providers_shared.so"
      "${onnxruntime_SOURCE_DIR}/lib/libonnxruntime_providers_cuda.so"
      "${onnxruntime_SOURCE_DIR}/lib/libonnxruntime_providers_tensorrt.so"
    )
  endif()
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${Onnxruntime_LINK_LIBS})
  target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM PUBLIC "${onnxruntime_SOURCE_DIR}/include")
  install(FILES ${Onnxruntime_INSTALL_LIBS} DESTINATION "${CMAKE_INSTALL_LIBDIR}/obs-plugins/${CMAKE_PROJECT_NAME}")

  # Create a symlink libonnxruntime.so.1 in the install folder
  if(NOT EXISTS "${CMAKE_BINARY_DIR}/libonnxruntime.so.1")
    file(CREATE_LINK "libonnxruntime.so.${Onnxruntime_VERSION}" "${CMAKE_BINARY_DIR}/libonnxruntime.so.1" SYMBOLIC)
  endif()

  install(
    FILES "${CMAKE_BINARY_DIR}/libonnxruntime.so.1"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/obs-plugins/${CMAKE_PROJECT_NAME}"
  )

  set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/${CMAKE_PROJECT_NAME}")
endif()
