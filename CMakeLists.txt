cmake_minimum_required(VERSION 3.19)
project(cstrike_hax LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)

# The current game build is x64, but keeping the x86 profile makes the ABI
# change easy to compare while following the earlier tutorial chapters.
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(NIKOOO777_ARCH x64)
    set(NIKOOO777_SIGNATURE_CONFIG
        "${CMAKE_SOURCE_DIR}/config/signatures-x64.ini")
    add_compile_definitions(NIKOOO777_ARCH_X64=1)
else()
    set(NIKOOO777_ARCH x86)
    set(NIKOOO777_SIGNATURE_CONFIG
        "${CMAKE_SOURCE_DIR}/config/signatures.ini")
    add_compile_definitions(NIKOOO777_ARCH_X86=1)
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
    Nikooo777/math/projection.cpp
    Nikooo777/netvars/netvars.cpp

    Nikooo777/sdk/create_interface.cpp

    Nikooo777/game/entity_list.cpp
    Nikooo777/game/interfaces.cpp
    Nikooo777/game/player.cpp
    Nikooo777/game/model.cpp
    Nikooo777/game/render_state.cpp
    Nikooo777/game/timing.cpp
    Nikooo777/game/weapon_math.cpp
    Nikooo777/game/weapon.cpp

    Nikooo777/features/aimbot.cpp
    Nikooo777/features/bhop.cpp
    Nikooo777/features/bone_esp.cpp
    Nikooo777/features/debug_info.cpp
    Nikooo777/features/fire_capture.cpp
    Nikooo777/features/menu.cpp
    Nikooo777/features/norecoil.cpp
    Nikooo777/features/perfect_nospread.cpp
    Nikooo777/features/triggerbot.cpp

    Nikooo777/hooks/create_move.cpp
    Nikooo777/hooks/client_fire_bullets.cpp
    Nikooo777/hooks/update_accuracy_penalty.cpp
    Nikooo777/hooks/d3d9_device.cpp
    Nikooo777/hooks/end_scene.cpp
    Nikooo777/hooks/hooks.cpp
    Nikooo777/hooks/lock_cursor.cpp
    Nikooo777/hooks/override_view.cpp
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

# Build MinHook from source for the selected architecture. The repository used
# to carry only prebuilt x86 libraries, which made an x64 build impossible and
# made the result depend on one particular MSVC runtime configuration.
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(MINHOOK_HDE minhook/src/hde/hde64.c)
else()
    set(MINHOOK_HDE minhook/src/hde/hde32.c)
endif()

add_library(minhook STATIC
    minhook/src/buffer.c
    minhook/src/hook.c
    minhook/src/trampoline.c
    ${MINHOOK_HDE}
)

target_include_directories(minhook PUBLIC
    ${CMAKE_SOURCE_DIR}/minhook/include
)
target_include_directories(minhook PRIVATE
    ${CMAKE_SOURCE_DIR}/minhook/src
    ${CMAKE_SOURCE_DIR}/minhook/src/hde
)

target_include_directories(nikooo777 PRIVATE
    ${CMAKE_SOURCE_DIR}/Nikooo777
    ${CMAKE_SOURCE_DIR}/minhook/include
    ${CMAKE_SOURCE_DIR}/imgui
    ${DXSDK_DIR}/Include
)

configure_file(
    "${NIKOOO777_SIGNATURE_CONFIG}"
    "${CMAKE_CURRENT_BINARY_DIR}/signatures.ini"
    COPYONLY
)

# d3d9 only — ImGui's DX9 backend does not need d3dx9.
target_link_libraries(nikooo777 PRIVATE
    minhook
    ${DXSDK_DIR}/Lib/${NIKOOO777_ARCH}/d3d9.lib
)

if (BUILD_TESTING)
    add_executable(weapon_math_tests
        tests/weapon_math_tests.cpp
        Nikooo777/game/weapon_math.cpp
        Nikooo777/math/vector.cpp
    )
    target_include_directories(weapon_math_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/Nikooo777
    )
    add_test(NAME weapon_math COMMAND weapon_math_tests)

    add_executable(projection_tests
        tests/projection_tests.cpp
        Nikooo777/math/projection.cpp
        Nikooo777/math/vector.cpp
    )
    target_include_directories(projection_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/Nikooo777
    )
    add_test(NAME projection COMMAND projection_tests)
endif()
