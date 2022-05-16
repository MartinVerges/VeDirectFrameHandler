
cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

project(VeDirectFrameHandler VERSION 1.0.1 DESCRIPTION "VE.Direct FrameHandler")

add_library(VeDirectFrameHandler SHARED VeDirectFrameHandler.cpp)
set_target_properties(VeDirectFrameHandler PROPERTIES PUBLIC_HEADER VeDirectFrameHandler.h)


# add the executable
#add_executable(VeDirectFrameHandler VeDirectFrameHandler.cpp)
