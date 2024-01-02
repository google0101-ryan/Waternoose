cmake_minimum_required(VERSION 3.2)

project(DOOM3BFG)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(SOURCES src/memory/memory.cpp
			src/main.cpp
			src/loader/xex.cpp)

set(AES_SOURCES src/crypto/rijndael-alg-fst.cpp)

include_directories(${CMAKE_SOURCE_DIR}/src)
add_executable(xbox360 ${SOURCES} ${AES_SOURCES})