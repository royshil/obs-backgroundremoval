cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED PLATFORM)
  if(APPLE)
    set(PLATFORM "macos")
  elseif(WIN32)
    set(PLATFORM "windows")
  else()
    set(PLATFORM "linux")
  endif()
endif()

file(REMOVE_RECURSE onnxruntime)

if(PLATFORM STREQUAL "macos")
  file(
    DOWNLOAD
      https://github.com/microsoft/onnxruntime/releases/download/v1.23.2/onnxruntime-osx-universal2-1.23.2.tgz
      onnxruntime-osx-universal2-1.23.2.tgz
    EXPECTED_HASH SHA256=49ae8e3a66ccb18d98ad3fe7f5906b6d7887df8a5edd40f49eb2b14e20885809
  )
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar xf onnxruntime-osx-universal2-1.23.2.tgz
  )
  file(RENAME onnxruntime-osx-universal2-1.23.2 onnxruntime)
elseif(PLATFORM STREQUAL "windows")
  file(
    DOWNLOAD
      https://github.com/microsoft/onnxruntime/releases/download/v1.23.2/onnxruntime-win-x64-gpu-1.23.2.zip
      onnxruntime-win-x64-gpu-1.23.2.zip
    EXPECTED_HASH SHA256=e77afdbbc2b8cb6da4e5a50d89841b48c44f3e47dce4fb87b15a2743786d0bb9
  )
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar xf onnxruntime-win-x64-gpu-1.23.2.zip
  )
  file(RENAME onnxruntime-win-x64-gpu-1.23.2 onnxruntime)
elseif(PLATFORM STREQUAL "linux")
  file(
    DOWNLOAD
      https://github.com/microsoft/onnxruntime/releases/download/v1.23.2/onnxruntime-linux-x64-1.23.2.tgz
      onnxruntime-linux-x64-1.23.2.tgz
    EXPECTED_HASH SHA256=1fa4dcaef22f6f7d5cd81b28c2800414350c10116f5fdd46a2160082551c5f9b
  )
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar xf onnxruntime-linux-x64-1.23.2.tgz
  )
  file(RENAME onnxruntime-linux-x64-1.23.2 onnxruntime)
  execute_process(COMMAND ln -s lib onnxruntime/lib64)
else()
  message(FATAL_ERROR "Unsupported platform: ${PLATFORM}")
endif()

if(EXISTS onnxruntime/lib/cmake/onnxruntime/onnxruntimeTargets.cmake)
  file(READ onnxruntime/lib/cmake/onnxruntime/onnxruntimeTargets.cmake FILE_CONTENT)

  set(OLD_STRING "INTERFACE_INCLUDE_DIRECTORIES \"\${_IMPORT_PREFIX}/include/onnxruntime\"")
  set(NEW_STRING "INTERFACE_INCLUDE_DIRECTORIES \"\${_IMPORT_PREFIX}/include\"")

  string(REPLACE "${OLD_STRING}" "${NEW_STRING}" MODIFIED_CONTENT "${FILE_CONTENT}")
  file(WRITE onnxruntime/lib/cmake/onnxruntime/onnxruntimeTargets.cmake "${MODIFIED_CONTENT}")
endif()
