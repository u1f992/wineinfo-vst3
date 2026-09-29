cmake_minimum_required(VERSION 3.16)
project(wineinfo_vst3 VERSION 1.0.0 LANGUAGES C CXX)

include(FetchContent)

set(VST3SDK_VERSION "v3.8.0_build_66")

FetchContent_Declare(
    vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG        ${VST3SDK_VERSION}
    GIT_SHALLOW    FALSE
    GIT_PROGRESS   TRUE
)

function(apply_patch_once patch working_directory)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} apply --reverse --check ${patch}
        WORKING_DIRECTORY ${working_directory}
        RESULT_VARIABLE already_applied
        OUTPUT_QUIET
        ERROR_QUIET
    )
    if(already_applied EQUAL 0)
        return()
    endif()

    execute_process(
        COMMAND ${GIT_EXECUTABLE} apply ${patch}
        WORKING_DIRECTORY ${working_directory}
        RESULT_VARIABLE patch_result
    )
    if(NOT patch_result EQUAL 0)
        message(FATAL_ERROR "Failed to apply ${patch}")
    endif()
endfunction()

FetchContent_GetProperties(vst3sdk)
if(NOT vst3sdk_POPULATED)
    FetchContent_Populate(vst3sdk)

    # Apply patches for cross-compiling with mingw-w64
    find_package(Git REQUIRED)
    apply_patch_once(${CMAKE_SOURCE_DIR}/patches/vst3sdk-cmake.patch ${vst3sdk_SOURCE_DIR}/cmake)
    apply_patch_once(${CMAKE_SOURCE_DIR}/patches/vst3sdk-public-sdk.patch ${vst3sdk_SOURCE_DIR}/public.sdk)
endif()

set(VST3_SDK_ROOT "${vst3sdk_SOURCE_DIR}")

# Disable VSTGUI
# The editor is drawn with Direct3D 11 and Direct2D (src/view_d3d11.cpp).
set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)

# Disable SDK examples
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)

# Disable automatic symlink creation to VST3 folder
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)

add_subdirectory("${VST3_SDK_ROOT}" "${CMAKE_BINARY_DIR}/vst3sdk")

set(SOURCES
  src/factory.cpp
  src/processor.cpp
  src/controller.cpp
  src/view_d3d11.cpp
  src/wineinfo.cpp
)

smtg_add_vst3plugin(wineinfo ${SOURCES})
target_include_directories(wineinfo PRIVATE src ${VST3_SDK_ROOT})
target_link_libraries(wineinfo PRIVATE sdk d3d11 d2d1 dwrite)

target_compile_definitions(wineinfo PRIVATE UNICODE _UNICODE)

# Workaround: VST3 SDK bundle creation assumes multi-config generators (Visual Studio)
# but Ninja is single-config. Manually set LIBRARY_OUTPUT_DIRECTORY for proper bundle structure.
if(SMTG_WIN AND SMTG_CREATE_BUNDLE_FOR_WINDOWS AND NOT CMAKE_CONFIGURATION_TYPES)
    get_target_property(WIN_ARCHITECTURE_NAME wineinfo SMTG_WIN_ARCHITECTURE_NAME)
    get_target_property(PLUGIN_PACKAGE_CONTENTS wineinfo SMTG_PLUGIN_PACKAGE_CONTENTS)
    get_target_property(PLUGIN_BINARY_DIR wineinfo SMTG_PLUGIN_BINARY_DIR)
    get_target_property(PLUGIN_PACKAGE_NAME wineinfo SMTG_PLUGIN_PACKAGE_NAME)
    set_target_properties(wineinfo
        PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY ${PLUGIN_BINARY_DIR}/${CMAKE_BUILD_TYPE}/${PLUGIN_PACKAGE_NAME}/${PLUGIN_PACKAGE_CONTENTS}/${WIN_ARCHITECTURE_NAME}
    )
endif()

smtg_target_configure_version_file(wineinfo)

add_executable(ui_harness
  harness/ui_harness.cpp
  ${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/module_win32.cpp
  ${VST3_SDK_ROOT}/public.sdk/source/vst/hosting/plugprovider.cpp
)
target_link_libraries(ui_harness PRIVATE sdk_hosting)

enable_testing()
add_test(NAME ui_harness
  COMMAND ui_harness --screenshot ${CMAKE_BINARY_DIR}/ui_harness.bmp $<TARGET_FILE:wineinfo>
)
