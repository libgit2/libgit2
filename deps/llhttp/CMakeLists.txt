file(GLOB SRC_HTTP "*.c" "*.h")
list(SORT SRC_HTTP)

add_library(llhttp OBJECT ${SRC_HTTP})

if(NOT MSVC)
	set_source_files_properties(api.c http.c llhttp.c PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter -Wno-missing-declarations")
endif()
