cmake_minimum_required(VERSION 3.5)
project(motion-filter)

include(external/FindLibObs.cmake)
find_package(LibObs REQUIRED)
set(motion-filter_SOURCES
	src/helper.c
	src/motion-filter.c
	)
	
set(motion-filter_HEADERS
	src/helper.h
	)	
	
add_library(motion-filter MODULE
	${motion-filter_SOURCES}
	${motion-filter_HEADERS})
	
target_link_libraries(motion-filter
	libobs)