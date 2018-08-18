cmake_minimum_required(VERSION 3.5)
project(motion-transition)

include(${CMAKE_SOURCE_DIR}/external/FindLibObs.cmake)
find_package(LibObs REQUIRED)
set(motion-transition_SOURCES
	../helper.c
	motion-transition.c
	)
	
set(motion-transition_HEADERS
	../helper.h
	)	
	
add_library(motion-transition MODULE
	${motion-transition_SOURCES}
	${motion-transition_HEADERS})
	
target_link_libraries(motion-transition
	libobs)

if(UNIX AND NOT APPLE)

	if(ARCH EQUAL 64)
		set(ARCH_NAME "x86_64")
	else()
		set(ARCH_NAME "i686")
	endif()

	set_target_properties(motion-transition PROPERTIES PREFIX "")

	install(TARGETS motion-transition
		LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/obs-plugins)
	install(DIRECTORY ${CMAKE_SOURCE_DIR}/data/motion-transition/
		DESTINATION "${CMAKE_INSTALL_PREFIX}/share/obs/obs-plugins/motion-transition/")
endif()