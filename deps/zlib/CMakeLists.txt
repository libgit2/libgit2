disable_warnings(implicit-fallthrough)

add_library(zlib STATIC
adler32.c
crc32.c
crc32.h
deflate.c
deflate.h
gzguts.h
infback.c
inffast.c
inffast.h
inffixed.h
inflate.c
inflate.h
inftrees.c
inftrees.h
trees.c
trees.h
zconf.h
zlib.h
zutil.c
zutil.h
)

if(MINGW OR MSYS)
	target_compile_definitions(zlib PRIVATE Z_HAVE_UNISTD_H _LFS64_LARGEFILE _LARGEFILE64_SOURCE=1)
endif()

target_link_directories(zlib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(zlib PRIVATE NO_VIZ STDC NO_GZIP HAVE_SYS_TYPES_H HAVE_STDINT_H HAVE_STDDEF_H)
