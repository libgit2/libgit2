add_library(zlib OBJECT)

disable_warnings(implicit-fallthrough)
add_definitions(-DNO_VIZ -DSTDC -DNO_GZIP -DHAVE_SYS_TYPES_H -DHAVE_STDINT_H -DHAVE_STDDEF_H)

if(MINGW OR MSYS)
	add_definitions(-DZ_HAVE_UNISTD_H -D_LFS64_LARGEFILE -D_LARGEFILE64_SOURCE=1)
endif()

set(ZLIB_SRC adler32.c
	crc32.c crc32.h
	deflate.c deflate.h
	gzguts.h
	infback.c
	inffast.c inffast.h
	inffixed.h
	inflate.c inflate.h
	inftrees.c inftrees.h
	trees.c trees.h
	zconf.h
	zlib.h
	zutil.c zutil.h)

include_directories(".")
target_sources(zlib PRIVATE ${ZLIB_SRC})
