add_library(reftable OBJECT ${SRC_REFTABLE})

file(GLOB SRC_REFTABLE "*.c" "*.h")
list(SORT SRC_REFTABLE)

set_property(TARGET reftable PROPERTY C_STANDARD 99)
target_sources(reftable PRIVATE ${SRC_REFTABLE})
target_include_directories(reftable PRIVATE include)
