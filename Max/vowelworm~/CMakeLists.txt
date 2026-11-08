# Builds vowelworm~.mxo with the official Max SDK (max-sdk-base, MIT licence).
#   cmake -S "Max/vowelworm~" -B build-max -DMAX_SDK_BASE=/path/to/max-sdk-base -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake_minimum_required(VERSION 3.19)

if(NOT MAX_SDK_BASE)
    message(FATAL_ERROR "Set MAX_SDK_BASE to a checkout of https://github.com/Cycling74/max-sdk-base")
endif()
get_filename_component(VW_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(C74_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/externals")

include(${MAX_SDK_BASE}/script/max-pretarget.cmake)

include_directories("${C74_INCLUDES}")
add_library(${PROJECT_NAME} MODULE vowelworm~.cpp ${VW_ROOT}/Source/Engine/Engine.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE ${VW_ROOT}/Source)

include(${MAX_SDK_BASE}/script/max-posttarget.cmake)
