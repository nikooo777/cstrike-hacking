cmake_minimum_required(VERSION 3.19)
project(cstrike_hax)
set(CMAKE_CXX_STANDARD 14)
file(GLOB_RECURSE nikooo777 CONFIGURE_DEPENDS "*.h" "*.cpp")
add_library(nikooo777 SHARED ${nikooo777} ${minhook} ${imgui})
target_link_libraries(
        nikooo777
        ${CMAKE_SOURCE_DIR}/minhook/lib/libMinHook-x86-v141-mdd.lib
        C:/Program\ Files\ \(x86\)/Microsoft\ DirectX\ SDK\ \(June\ 2010\)/Lib/x86/d3d9.lib
        C:/Program\ Files\ \(x86\)/Microsoft\ DirectX\ SDK\ \(June\ 2010\)/Lib/x86/d3dx9.lib
)
include_directories(
        C:/Program\ Files\ \(x86\)/Microsoft\ DirectX\ SDK\ \(June\ 2010\)/Include
)
