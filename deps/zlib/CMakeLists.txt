ADD_LIBRARY(zlib STATIC adler32.c crc32.c deflate.c inffast.c inflate.c inftrees.c trees.c zutil.c)
ADD_DEFINITIONS(-DNO_VIZ -DSTDC -DNO_GZIP)
