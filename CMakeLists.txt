cmake_minimum_required(VERSION 3.5)
project(motion-effect)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
add_subdirectory(src/motion-filter)
add_subdirectory(src/motion-transition)

