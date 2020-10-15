file(GLOB SRC_ZLIB "*.c" "*.h")
list(SORT SRC_ZLIB)

add_library(zlib OBJECT ${SRC_ZLIB})
target_include_directories(zlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(zlib PRIVATE NO_VIZ STDC NO_GZIP)
target_disable_warning(zlib implicit-fallthrough)
