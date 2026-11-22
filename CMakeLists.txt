cmake_minimum_required(VERSION 3.18)
project(ptr_inject CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ptr_inject src/main/jni/inject.cpp)
set_target_properties(ptr_inject PROPERTIES OUTPUT_NAME "ptr_inject.sh")
target_compile_options(ptr_inject PRIVATE -fno-rtti -fvisibility=hidden -ffunction-sections -fdata-sections -w)
target_link_options(ptr_inject PRIVATE -Wl,--gc-sections -Wl,--strip-all -static-libstdc++)
