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

FetchContent_GetProperties(vst3sdk)
if(NOT vst3sdk_POPULATED)
    FetchContent_Populate(vst3sdk)

    # Apply patches for cross-compiling with mingw-w64
    find_package(Git REQUIRED)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} apply ${CMAKE_SOURCE_DIR}/patches/vst3sdk-cmake.patch
        WORKING_DIRECTORY ${vst3sdk_SOURCE_DIR}/cmake
        RESULT_VARIABLE patch_result1
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} apply ${CMAKE_SOURCE_DIR}/patches/vst3sdk-public-sdk.patch
        WORKING_DIRECTORY ${vst3sdk_SOURCE_DIR}/public.sdk
        RESULT_VARIABLE patch_result2
    )
endif()

set(VST3_SDK_ROOT "${vst3sdk_SOURCE_DIR}")

# Disable VSTGUI
# This plugin only displays static text, so we use DAW's generic parameter UI instead.
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
  src/wineinfo.cpp
)

smtg_add_vst3plugin(wineinfo ${SOURCES})
target_include_directories(wineinfo PRIVATE src ${VST3_SDK_ROOT})
target_link_libraries(wineinfo PRIVATE sdk)

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
