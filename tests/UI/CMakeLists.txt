# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

add_executable(AboutDialogRunner AboutDialogRunner.cpp)
target_include_directories(AboutDialogRunner PRIVATE "${PLUGIN_SOURCE_DIR}")
target_link_libraries(AboutDialogRunner PRIVATE Qt6::Widgets)
set_target_properties(AboutDialogRunner PROPERTIES AUTORCC ON)

target_sources(
  AboutDialogRunner
  PRIVATE "${PLUGIN_SOURCE_DIR}/UI/AboutDialog.cpp" "${PLUGIN_SOURCE_DIR}/Resources/resources.qrc"
)

if(COMMAND collect_licenses)
  set(test_legal_header "${CMAKE_CURRENT_BINARY_DIR}/legal.hpp")
  collect_licenses("${test_legal_header}")
  target_sources(AboutDialogRunner PRIVATE "${test_legal_header}")
  target_include_directories(AboutDialogRunner PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
endif()

if(WIN32)
  set(obs_ui_dir "${PROJECT_SOURCE_DIR}/vendor/obs-studio/UI")
  # Reuse the pinned OBS widgets; replace only their application-service include
  # in build-tree copies, leaving the submodule unchanged.
  foreach(source_name properties-view qt-wrappers)
    set(source_path "${obs_ui_dir}/${source_name}.cpp")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source_path}")
    file(READ "${source_path}" source_text)
    string(FIND "${source_text}" "#include \"obs-app.hpp\"" app_include)
    if(app_include EQUAL -1)
      message(FATAL_ERROR "OBS ${source_name} application dependency changed; review the UI adapter")
    endif()
    string(REPLACE "#include \"obs-app.hpp\"" "#include \"PropertyUIApp.hpp\"" source_text "${source_text}")
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/obs-property-widgets/${source_name}.cpp" CONTENT "${source_text}")
  endforeach()

  add_executable(
    BRPropUI
    EXCLUDE_FROM_ALL
    BRPropUI.cpp
    PropertyUIApp.hpp
    "${CMAKE_CURRENT_BINARY_DIR}/obs-property-widgets/properties-view.cpp"
    "${CMAKE_CURRENT_BINARY_DIR}/obs-property-widgets/qt-wrappers.cpp"
    "${obs_ui_dir}/properties-view.hpp"
    "${obs_ui_dir}/properties-view.moc.hpp"
    "${obs_ui_dir}/vertical-scroll-area.cpp"
    "${obs_ui_dir}/double-slider.cpp"
    "${obs_ui_dir}/slider-ignorewheel.cpp"
    "${obs_ui_dir}/spinbox-ignorewheel.cpp"
    "${obs_ui_dir}/plain-text-edit.cpp"
    "${PLUGIN_SOURCE_DIR}/UI/AboutDialog.cpp"
    "${PLUGIN_SOURCE_DIR}/Resources/resources.qrc"
    "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/windows/FilterContext.cpp"
    "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/windows/ModuleText.cpp"
    "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/windows/AutoConfigurator.cpp"
    "${PLUGIN_SOURCE_DIR}/UpdateConfig/UpdateConfigSemver.cpp"
    "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/windows/PipelineFactory.cpp"
    "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/windows/MPLD10Pipeline.cpp"
    "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/windows/MediaPipeLandscapeDirectML10/MPLD10Engine.cpp"
    "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/windows/TraceLoggingProvider.cpp"
    "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/windows/generated/MediaPipeLandscapeDmlProgram.generated.cpp"
    "${PLUGIN_SOURCE_DIR}/DmlProgram/BaselineDmlCompiler.cpp"
    "${PLUGIN_SOURCE_DIR}/DmlProgram/BaselineDmlProgramRuntime.cpp"
  )

  set_target_properties(BRPropUI PROPERTIES AUTOMOC ON AUTORCC ON)
  target_include_directories(
    BRPropUI
    PRIVATE
      "${PLUGIN_SOURCE_DIR}"
      "${obs_ui_dir}"
      "${PROJECT_SOURCE_DIR}/vendor/obs-studio/deps/w32-pthreads"
      "${CMAKE_CURRENT_SOURCE_DIR}"
      "${CMAKE_CURRENT_BINARY_DIR}"
  )

  target_compile_definitions(
    BRPropUI
    PRIVATE
      PLUGIN_PRIVATE=public
      PLUGIN_VERSION_STR="${PLUGIN_VERSION}"
      OBS_LOG_HEADER="[PropertyUI] "
      FILTER_PROPERTY_LOCALE="${PROJECT_SOURCE_DIR}/data/locale/en-US.ini"
      OBS_FRONTEND_LOCALE="${obs_ui_dir}/data/locale/en-US.ini"
  )

  target_link_libraries(
    BRPropUI
    PRIVATE Qt6::Widgets CTest::libobs d3d11 d3d12 dxgi d3dcompiler directml advapi32
  )
endif()
