FILE(GLOB SRC_HTTP "*.c" "*.h")

ADD_LIBRARY(http-parser STATIC ${SRC_HTTP})
