cmake_minimum_required(VERSION 3.2)

project(DOOM3BFG)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE DEBUG)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(SOURCES src/memory/memory.cpp
			src/main.cpp
			src/loader/xex.cpp
			src/cpu/CPU.cpp
			src/cpu/ops.cpp
			src/kernel/kernel.cpp
			src/kernel/modules/xboxkrnl.cpp
			src/vfs/VFS.cpp)

set(AES_SOURCES src/crypto/rijndael-alg-fst.cpp)
set(LZX_SOURCES src/thirdparty/lzxd.cpp
				src/thirdparty/system.cpp
				src/loader/lzx.cpp)

include_directories(${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/thirdparty)
add_executable(xbox360 ${SOURCES} ${AES_SOURCES} ${LZX_SOURCES})