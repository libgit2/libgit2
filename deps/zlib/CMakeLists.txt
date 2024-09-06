add_library(zlib OBJECT ${SRC_ZLIB})

target_compile_definitions(zlib PRIVATE DNO_VIZ STDC NO_GZIP HAVE_SYS_TYPES_H HAVE_STDINT_H HAVE_STDDEF_H)

if(MINGW OR MSYS)
	target_compile_definitions(zlib Z_HAVE_UNISTD_H _LFS64_LARGEFILE -LARGEFILE64_SOURCE=1)
endif()

disable_warnings(implicit-fallthrough)

file(GLOB SRC_ZLIB "*.c" "*.h")
list(SORT SRC_ZLIB)

target_sources(zlib PRIVATE ${SRC_ZLIB})
