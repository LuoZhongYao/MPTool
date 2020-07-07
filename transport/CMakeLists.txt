add_library(transport)
if(WIN32)
	set(TRANSPORT_OS_LIBRARY hidapi)
else(WIN32)
	set(TRANSPORT_OS_LIBRARY hidapi-hidraw)
	set(TRANSPORT_OS_SOURCES
		baudrate.c
		serial_transport.c
		)
endif(WIN32)

target_link_libraries(transport PRIVATE usb-1.0 ${TRANSPORT_OS_LIBRARY})
target_include_directories(transport INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(transport PRIVATE
	transport.c
	${TRANSPORT_OS_SOURCES}
	hidapi_transport.c
	usb_transport.c
	mcu_transport.c
	)

