add_library(llhttp OBJECT)

set(LLHTTP_SRC api.c http.c llhttp.c llhttp.h)
target_sources(llhttp PRIVATE ${LLHTTP_SRC})

if(NOT MSVC)
	set_source_files_properties(api.c http.c llhttp.c PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter -Wno-missing-declarations")
endif()
