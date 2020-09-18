file(GLOB SRC_HTTP "*.c" "*.h")
list(SORT SRC_HTTP)

add_library(http-parser OBJECT ${SRC_HTTP})
target_enable_warning(http-parser implicit-fallthrough=1)
