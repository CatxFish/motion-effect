cmake_minimum_required(VERSION 3.5)
project(motion-filter)

include(external/FindLibObs.cmake)
find_package(LibObs REQUIRED)
set(motion-filter_SOURCES
	src/motion-filter.c
	)
	
add_library(motion-filter MODULE
	${motion-filter_SOURCES})
	
target_link_libraries(motion-filter
	libobs)