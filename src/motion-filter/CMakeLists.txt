cmake_minimum_required(VERSION 3.5)
project(motion-filter)

include(${CMAKE_SOURCE_DIR}/external/FindLibObs.cmake)
find_package(LibObs REQUIRED)
set(motion-filter_SOURCES
	../helper.c
	motion-filter.c
	)
	
set(motion-filter_HEADERS
	../helper.h
	)	
	
include_directories(
	"${LIBOBS_INCLUDE_DIR}/../UI/obs-frontend-api")	
	
add_library(motion-filter MODULE
	${motion-filter_SOURCES}
	${motion-filter_HEADERS})
	
target_link_libraries(motion-filter
	libobs)

if(UNIX AND NOT APPLE)

	if(ARCH EQUAL 64)
		set(ARCH_NAME "x86_64")
	else()
		set(ARCH_NAME "i686")
	endif()

	set_target_properties(motion-filter PROPERTIES PREFIX "")

	install(TARGETS motion-filter
		LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/obs-plugins)
	install(DIRECTORY ${CMAKE_SOURCE_DIR}/data/motion-filter/
		DESTINATION "${CMAKE_INSTALL_PREFIX}/share/obs/obs-plugins/motion-filter/")
endif()

	
if(WIN32)
	set(OBS_FRONTEND_LIB "OBS_FRONTEND_LIB-NOTFOUND" CACHE FILEPATH "OBS frontend library")
	if(OBS_FRONTEND_LIB EQUAL "OBS_FRONTEND_LIB-NOTFOUND")
		message(FATAL_ERROR "OBS_FRONTEND_LIB NOTFOUND")
	endif()
	
		target_link_libraries(motion-filter
		"${OBS_FRONTEND_LIB}")
				
endif()

