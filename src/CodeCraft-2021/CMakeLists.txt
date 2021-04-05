# CMake 最低版本号要求
cmake_minimum_required(VERSION 2.8)

# 项目信息
project("CodeCraft-2021")

# include路径
include_directories(${PROJECT_SOURCE_DIR}/lib)

# 设置可执行文件生成路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/../bin)

# 生成debug版本
# SET(CMAKE_BUILD_TYPE "debug")
# 生成release版本
SET(CMAKE_BUILD_TYPE "release")
if (CMAKE_BUILD_TYPE STREQUAL debug)
    add_definitions(-DTEST)
endif ()

SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -march=native -O3 -g -ggdb -std=c++17")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -march=native -O3  -std=c++17")

# 查找当前目录下的所有源文件
# 并将名称保存到 DIR_LIB_SRCS 变量

option(SUBMIT_MODE "enable submit to speed " ON)

if(SUBMIT_MODE)
    set(DIR_SRCS tmp/merged_file.cpp) # SUBMIT MODE
else()
    aux_source_directory(. DIR_SRCS) # DEBUG MODE
endif()

#SET(CMAKE_C_COMPILER "/usr/bin/gcc-9")
#SET(CMAKE_CXX_COMPILER "/usr/bin/g++-9")

# 指定生成目标
add_executable(CodeCraft-2021 ${DIR_SRCS})