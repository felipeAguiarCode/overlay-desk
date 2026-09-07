cmake_minimum_required(VERSION 3.25)

project(OverlayDesk
    VERSION 0.1.0
    DESCRIPTION "Overlay Desk - GPU-first window overlay with realtime filters"
    LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Overlay Desk is Windows-only (ADR-0001).")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static CRT: the product ships as a portable executable (Milestone 10), so it must not
# depend on the Visual C++ redistributable being installed.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# --------------------------------------------------------------------------------------
# Windows SDK discovery
#
# Three things come straight out of the SDK:
#   * the pre-generated C++/WinRT projection headers  (Include/<ver>/cppwinrt)
#   * fxc.exe                                         (bin/<ver>/x64)
#   * the usual um/shared headers and import libraries
# --------------------------------------------------------------------------------------

cmake_host_system_information(RESULT WINDOWS_KITS_ROOT
    QUERY WINDOWS_REGISTRY "HKLM/SOFTWARE/Microsoft/Windows Kits/Installed Roots"
    VALUE "KitsRoot10"
    VIEW BOTH
    ERROR_VARIABLE WINDOWS_KITS_ROOT_ERROR)

if(NOT WINDOWS_KITS_ROOT OR NOT EXISTS "${WINDOWS_KITS_ROOT}")
    set(WINDOWS_KITS_ROOT "$ENV{ProgramFiles(x86)}/Windows Kits/10")
endif()
file(TO_CMAKE_PATH "${WINDOWS_KITS_ROOT}" WINDOWS_KITS_ROOT)
string(REGEX REPLACE "/+$" "" WINDOWS_KITS_ROOT "${WINDOWS_KITS_ROOT}")

if(NOT EXISTS "${WINDOWS_KITS_ROOT}/Include")
    message(FATAL_ERROR "Windows SDK not found under ${WINDOWS_KITS_ROOT}. ${WINDOWS_KITS_ROOT_ERROR}")
endif()

# Pick the newest installed SDK that actually carries the C++/WinRT projection headers.
set(OVERLAYDESK_SDK_VERSION "")
file(GLOB _sdk_versions RELATIVE "${WINDOWS_KITS_ROOT}/Include" "${WINDOWS_KITS_ROOT}/Include/10.*")
list(SORT _sdk_versions COMPARE NATURAL ORDER DESCENDING)
foreach(_v IN LISTS _sdk_versions)
    if(EXISTS "${WINDOWS_KITS_ROOT}/Include/${_v}/cppwinrt/winrt/Windows.Graphics.Capture.h")
        set(OVERLAYDESK_SDK_VERSION "${_v}")
        break()
    endif()
endforeach()

if(NOT OVERLAYDESK_SDK_VERSION)
    message(FATAL_ERROR
        "No Windows SDK with C++/WinRT projection headers found under "
        "${WINDOWS_KITS_ROOT}/Include. Install a Windows 10 SDK 10.0.17763 or newer that "
        "ships the cppwinrt projection.")
endif()

set(OVERLAYDESK_CPPWINRT_INCLUDE_DIR "${WINDOWS_KITS_ROOT}/Include/${OVERLAYDESK_SDK_VERSION}/cppwinrt")
message(STATUS "Windows SDK root:    ${WINDOWS_KITS_ROOT}")
message(STATUS "Windows SDK version: ${OVERLAYDESK_SDK_VERSION}")

include(CompileShaders)

# --------------------------------------------------------------------------------------
# Version resource
#
# Generated from assets/OverlayDesk.rc.in so the version is single-sourced from project().
# --------------------------------------------------------------------------------------

set(OVERLAYDESK_COMPANY "" CACHE STRING "CompanyName shown in the executable's version resource")
set(OVERLAYDESK_COPYRIGHT "Copyright (c) 2026 Felipe Aguiar. MIT License."
    CACHE STRING "LegalCopyright shown in the executable's version resource")

# rc.exe reads the path out of the generated .rc, which lives in the build tree, so it has to
# be absolute. Forward slashes are what rc accepts without escaping.
set(OVERLAYDESK_ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/OverlayDesk.ico")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/OverlayDesk.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/OverlayDesk.rc"
    @ONLY)

# --------------------------------------------------------------------------------------
# Third-party dependencies
# --------------------------------------------------------------------------------------

include(FetchContent)

# Dear ImGui has no CMakeLists of its own, so we compile the translation units we need.
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.9b
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp")
target_include_directories(imgui SYSTEM PUBLIC
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends")
target_compile_definitions(imgui PUBLIC
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(imgui PRIVATE /W0)
target_link_libraries(imgui PUBLIC d3d11 dxgi dwmapi)
set_target_properties(imgui PROPERTIES FOLDER "ThirdParty")

# nlohmann/json: only the amalgamated header. Pulling the upstream project in through
# add_subdirectory is not an option here - its cmake_minimum_required predates 3.5, which
# CMake 4.x rejects outright.
FetchContent_Declare(nlohmann_json_header
    URL      https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp
    URL_HASH SHA256=aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63
    DOWNLOAD_NO_EXTRACT TRUE)
FetchContent_MakeAvailable(nlohmann_json_header)

set(_json_include_root "${CMAKE_BINARY_DIR}/external")
# COPY_FILE will not create intermediate directories.
file(MAKE_DIRECTORY "${_json_include_root}/nlohmann")
file(COPY_FILE
    "${nlohmann_json_header_SOURCE_DIR}/json.hpp"
    "${_json_include_root}/nlohmann/json.hpp"
    ONLY_IF_DIFFERENT)

add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json SYSTEM INTERFACE "${_json_include_root}")

# --------------------------------------------------------------------------------------
# OverlayDesk
# --------------------------------------------------------------------------------------

add_executable(OverlayDesk WIN32
    src/main.cpp
    src/Application.cpp
    src/Application.h
    src/core/AppState.h
    src/core/Hotkeys.cpp
    src/core/Hotkeys.h
    src/core/RenderMath.h
    src/core/PresetRepository.cpp
    src/core/PresetRepository.h
    src/core/Settings.cpp
    src/core/Settings.h
    src/core/SettingsJson.h
    src/windowing/ControlWindow.cpp
    src/windowing/ControlWindow.h
    src/windowing/HotkeyManager.cpp
    src/windowing/HotkeyManager.h
    src/windowing/OverlayWindow.cpp
    src/windowing/OverlayWindow.h
    src/windowing/TargetWindow.cpp
    src/windowing/TargetWindow.h
    src/windowing/WindowTracker.cpp
    src/windowing/WindowTracker.h
    src/capture/CaptureSession.cpp
    src/capture/CaptureSession.h
    src/graphics/D3D11Device.cpp
    src/graphics/D3D11Device.h
    src/graphics/RenderResources.cpp
    src/graphics/RenderResources.h
    src/graphics/Renderer.cpp
    src/graphics/Renderer.h
    src/graphics/ShaderManager.cpp
    src/graphics/ShaderManager.h
    src/graphics/SwapChain.cpp
    src/graphics/SwapChain.h
    src/ui/ControlPanel.cpp
    src/ui/ControlPanel.h
    src/ui/EffectsPage.cpp
    src/ui/PresetsPage.cpp
    src/ui/FiltersPage.cpp
    src/ui/HotkeysPage.cpp
    src/ui/OverlayPage.cpp
    src/ui/TargetPage.cpp
    src/util/ComHelpers.h
    src/util/CrashHandler.cpp
    src/util/CrashHandler.h
    src/util/Log.cpp
    src/util/Log.h
    src/util/Win32Helpers.cpp
    src/util/Win32Helpers.h
    app.manifest
    "${CMAKE_CURRENT_BINARY_DIR}/generated/OverlayDesk.rc")

target_include_directories(OverlayDesk PRIVATE src)
target_include_directories(OverlayDesk SYSTEM PRIVATE "${OVERLAYDESK_CPPWINRT_INCLUDE_DIR}")

target_compile_definitions(OverlayDesk PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    OVERLAYDESK_VERSION="${PROJECT_VERSION}"
    OVERLAYDESK_SHADER_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders")

target_compile_options(OverlayDesk PRIVATE
    /W4 /permissive- /utf-8 /EHsc /Zc:__cplusplus /Zc:preprocessor /MP
    $<$<NOT:$<CONFIG:Debug>>:/WX>
    $<$<NOT:$<CONFIG:Debug>>:/GL>)
target_link_options(OverlayDesk PRIVATE
    $<$<NOT:$<CONFIG:Debug>>:/LTCG>
    $<$<NOT:$<CONFIG:Debug>>:/OPT:REF>
    $<$<NOT:$<CONFIG:Debug>>:/OPT:ICF>)

target_link_libraries(OverlayDesk PRIVATE
    imgui
    nlohmann_json
    d3d11          # Direct3D 11 device / context
    dxgi           # swap chains, adapters
    dcomp          # DirectComposition (ADR-0006)
    dwmapi         # DWM window attributes: cloaked test, extended frame bounds
    windowsapp     # C++/WinRT: Windows.Graphics.Capture
    coremessaging  # CreateDispatcherQueueController
    d3dcompiler    # Debug-only shader hot reload
    shcore
    user32
    shell32
    ole32)

add_hlsl_shader(OverlayDesk
    SOURCE   "${CMAKE_CURRENT_SOURCE_DIR}/shaders/OverlayDeskVS.hlsl"
    PROFILE  vs_5_0
    ENTRY    main
    VARIABLE g_OverlayDeskVS
    DEPENDS  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/Common.hlsli")

add_hlsl_shader(OverlayDesk
    SOURCE   "${CMAKE_CURRENT_SOURCE_DIR}/shaders/OverlayDeskPS.hlsl"
    PROFILE  ps_5_0
    ENTRY    main
    VARIABLE g_OverlayDeskPS
    DEPENDS  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/Common.hlsli")

set_target_properties(OverlayDesk PROPERTIES
    VS_DPI_AWARE "PerMonitor"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "src" FILES
    src/main.cpp src/Application.cpp src/Application.h)

# --------------------------------------------------------------------------------------
# Tests
# --------------------------------------------------------------------------------------

enable_testing()
add_subdirectory(tests)
