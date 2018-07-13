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

if(UNIX AND NOT APPLE)

	if(ARCH EQUAL 64)
		set(ARCH_NAME "x86_64")
	else()
		set(ARCH_NAME "i686")
	endif()

	set_target_properties(motion-filter PROPERTIES PREFIX "")

	install(TARGETS motion-filter
		LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/obs-plugins)
	install(DIRECTORY locale/
		DESTINATION "${CMAKE_INSTALL_PREFIX}/share/obs/obs-plugins/motion-filter/locale")
endif()
