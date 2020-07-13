add_library(transport)
if(MINGW)
	set(TRANSPORT_OS_LIBRARY hidapi)
	set(TRANSPORT_OS_SOURCES com_transport.c)
else(MINGW)
	set(TRANSPORT_OS_LIBRARY hidapi-hidraw)
	set(TRANSPORT_OS_SOURCES baudrate.c serial_transport.c)
endif(MINGW)

target_link_libraries(transport PRIVATE usb-1.0 ${TRANSPORT_OS_LIBRARY})
target_include_directories(transport INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(transport PRIVATE
	transport.c
	${TRANSPORT_OS_SOURCES}
	hidapi_transport.c
	usb_transport.c
	mcu_transport.c
	)
