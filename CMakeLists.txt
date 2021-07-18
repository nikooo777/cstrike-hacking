cmake_minimum_required(VERSION 3.19)
project(cstrike_hax)
set(CMAKE_CXX_STANDARD 14)

add_library(nikooo777 SHARED Nikooo777/source.cpp Nikooo777/CBasePlayer.h Nikooo777/mem/mem.cpp Nikooo777/mem/mem.h Nikooo777/ClientState.h)
