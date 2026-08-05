cmake_minimum_required(VERSION 3.19)
project(cstrike_hax LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CS:S is 32-bit. Prefer MSVC Win32 (matches MinHook v141 x86 libs).
# CLion's bundled MinGW is typically x86_64-only and cannot produce a CSS inject DLL.
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(WARNING "This is a 64-bit toolchain. CS:S needs a 32-bit (Win32/x86) DLL. "
                    "Use VS Build Tools: vcvarsall x86, or a CLion MSVC Win32 profile.")
endif()

# DirectX SDK (June 2010) — override with -DDXSDK_DIR=... if installed elsewhere.
if (NOT DEFINED DXSDK_DIR)
    set(DXSDK_DIR "C:/Program Files (x86)/Microsoft DirectX SDK (June 2010)")
endif()

# --- Project sources (explicit: easy to see what belongs in the DLL) ---
set(NIKOOO777_SOURCES
    Nikooo777/dllmain.cpp

    Nikooo777/core/modules.cpp

    Nikooo777/config/config.cpp
    Nikooo777/memory/mem.cpp
    Nikooo777/math/vector.cpp
    Nikooo777/netvars/netvars.cpp

    Nikooo777/sdk/create_interface.cpp

    Nikooo777/game/entity_list.cpp
    Nikooo777/game/interfaces.cpp
    Nikooo777/game/player.cpp

    Nikooo777/features/aimbot.cpp
    Nikooo777/features/bhop.cpp
    Nikooo777/features/debug_info.cpp
    Nikooo777/features/menu.cpp
    Nikooo777/features/norecoil.cpp
    Nikooo777/features/triggerbot.cpp

    Nikooo777/hooks/create_move.cpp
    Nikooo777/hooks/d3d9_device.cpp
    Nikooo777/hooks/end_scene.cpp
    Nikooo777/hooks/frame_stage_notify.cpp
    Nikooo777/hooks/hooks.cpp
)

# Dear ImGui (core + DX9 / Win32 backends; demo intentionally omitted)
set(IMGUI_SOURCES
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_impl_dx9.cpp
    imgui/imgui_impl_win32.cpp
)

add_library(nikooo777 SHARED ${NIKOOO777_SOURCES} ${IMGUI_SOURCES})

target_include_directories(nikooo777 PRIVATE
    ${CMAKE_SOURCE_DIR}/Nikooo777
    ${CMAKE_SOURCE_DIR}/minhook/includes
    ${CMAKE_SOURCE_DIR}/imgui
    ${DXSDK_DIR}/Include
)

configure_file(
    "${CMAKE_SOURCE_DIR}/config/signatures.ini"
    "${CMAKE_CURRENT_BINARY_DIR}/signatures.ini"
    COPYONLY
)

# Debug CRT MinHook by default (matches previous setup).
set(MINHOOK_LIB "${CMAKE_SOURCE_DIR}/minhook/lib/libMinHook-x86-v141-mdd.lib")
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(MINHOOK_LIB "${CMAKE_SOURCE_DIR}/minhook/lib/libMinHook-x86-v141-md.lib")
endif()

# d3d9 only — ImGui's DX9 backend does not need d3dx9.
target_link_libraries(nikooo777 PRIVATE
    ${MINHOOK_LIB}
    ${DXSDK_DIR}/Lib/x86/d3d9.lib
)
