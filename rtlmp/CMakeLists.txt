add_library(rtlmp)
target_link_libraries(rtlmp PRIVATE transport)
target_include_directories(rtlmp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(rtlmp PRIVATE
	crc16.c
	rtlbt.c
	rtlmp.c
	rtlimg.c
	rtlmptool.c
	)

