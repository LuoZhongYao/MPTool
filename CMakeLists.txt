project(MPTool)
cmake_minimum_required(VERSION 3.8)
set(CMAKE_C_FLAGS "-g -fsanitize=address")

add_executable(MPTool)

target_sources(MPTool PRIVATE
	main.c
	baudrate.c
	rtlmp.c
	rtlbt.c
	rtlimg.c
	usb_transport.c
	serial_transport.c
	)

target_link_libraries(MPTool PRIVATE usb-1.0)
