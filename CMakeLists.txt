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
	
include_directories(
	"${LIBOBS_INCLUDE_DIR}/../UI/obs-frontend-api")	
	
add_library(motion-filter MODULE
	${motion-filter_SOURCES}
	${motion-filter_HEADERS})
	
target_link_libraries(motion-filter
	libobs)
	
	
if(WIN32)
	set(OBS_FRONTEND_LIB "OBS_FRONTEND_LIB-NOTFOUND" CACHE FILEPATH "OBS frontend library")
	if(OBS_FRONTEND_LIB EQUAL "OBS_FRONTEND_LIB-NOTFOUND")
		message(FATAL_ERROR "OBS_FRONTEND_LIB NOTFOUND")
	endif()
	
		target_link_libraries(motion-filter
		"${OBS_FRONTEND_LIB}")
				
endif()