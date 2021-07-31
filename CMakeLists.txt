cmake_minimum_required(VERSION 3.19)
project(cstrike_hax)
set(CMAKE_CXX_STANDARD 14)

add_library(nikooo777 SHARED Nikooo777/source.cpp Nikooo777/CBasePlayer.h Nikooo777/mem/mem.cpp Nikooo777/mem/mem.h Nikooo777/ClientState.h Nikooo777/CLocal.h Nikooo777/vector.h Nikooo777/padding.h Nikooo777/misc.h Nikooo777/offsets.h Nikooo777/hack.h Nikooo777/hack.cpp Nikooo777/common.h Nikooo777/Helper.cpp Nikooo777/Helper.h Nikooo777/norecoil.h Nikooo777/norecoil.cpp Nikooo777/bhop.h Nikooo777/bhop.cpp Nikooo777/triggerbot.cpp Nikooo777/triggerbot.h Nikooo777/aimbot.cpp Nikooo777/aimbot.h Nikooo777/vector.cpp)
