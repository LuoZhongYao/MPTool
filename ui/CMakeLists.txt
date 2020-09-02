add_executable(MPTool)
target_sources(MPTool PRIVATE main.c)
target_link_libraries(MPTool PRIVATE rtlmp transport)
 
add_executable(MPToolGui)
target_sources(MPToolGui PRIVATE guimain.c)

find_package (PkgConfig REQUIRED)
if(MINGW)

	pkg_check_modules (GTK3 REQUIRED gtk+-win32-3.0)
	ENABLE_LANGUAGE(RC)
	set(CMAKE_RC_COMPILE_OBJECT
		"<CMAKE_RC_COMPILER> <FLAGS> -O coff <DEFINES> -i <SOURCE> -o <OBJECT>")

	target_sources(MPToolGui PRIVATE gui.rc)
else(MINGW)

pkg_check_modules (GTK3 REQUIRED gtk+-3.0)

endif(MINGW)

target_link_libraries(MPToolGui PRIVATE rtlmp transport ${GTK3_STATIC_LIBRARIES})
target_include_directories(MPToolGui PRIVATE ${GTK3_INCLUDE_DIRS})
target_link_options(MPToolGui PRIVATE -Wl,-subsystem,windows -Wl,--wrap,printf)
