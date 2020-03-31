add_library(reftable OBJECT ${SRC_REFTABLE})

target_compile_definitions(reftable PRIVATE REFTABLE_STANDALONE)

disable_warnings(sign-compare)
disable_warnings(unused-parameter)

file(GLOB SRC_REFTABLE "*.c" "*.h")
list(SORT SRC_REFTABLE)

target_sources(reftable PRIVATE ${SRC_REFTABLE})
target_include_directories(reftable PRIVATE include)
