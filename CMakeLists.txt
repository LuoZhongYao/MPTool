project(MPTool)
cmake_minimum_required(VERSION 3.8)
set(CMAKE_C_FLAGS_DEBUG "-g -fsanitize=address")

if(WIN32)
	set(MPTOOL_SOURCES winmain.c)
	set(RTLMP_OS_LIBRARY hidapi)
	add_library(rtlmp SHARED)
else(WIN32)
	set(MPTOOL_SOURCES main.c)
	set(RTLMP_OS_SOURCES
		baudrate.c
		usb_transport.c
		serial_transport.c
		)

	add_library(rtlmp STATIC)
	set(RTLMP_OS_LIBRARY hidapi-hidraw)
endif(WIN32)

target_sources(rtlmp PRIVATE
	crc16.c
	rtlbt.c
	rtlmp.c
	rtlimg.c
	rtlmptool.c
	${RTLMP_OS_SOURCES}
	hidapi_transport.c
	usb_transport.c
	mcu_transport.c
	)

target_link_libraries(rtlmp PRIVATE usb-1.0 ${RTLMP_OS_LIBRARY})

add_executable(MPTool)
target_sources(MPTool PRIVATE ${MPTOOL_SOURCES})
target_link_libraries(MPTool PRIVATE rtlmp)
