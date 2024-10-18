add_library(llhttp STATIC
	api.c
	http.c
	llhttp.c
	llhttp.h
)

target_include_directories(llhttp PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

if(NOT MSVC)
	set_source_files_properties(api.c http.c llhttp.c PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter -Wno-missing-declarations")
endif()
