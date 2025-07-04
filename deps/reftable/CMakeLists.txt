add_library(reftable OBJECT ${SRC_REFTABLE})

file(GLOB SRC_REFTABLE "*.c" "*.h")
list(SORT SRC_REFTABLE)

set_property(TARGET reftable PROPERTY C_STANDARD 99)
target_sources(reftable PRIVATE ${SRC_REFTABLE})
target_include_directories(reftable PRIVATE
	include
	"${PROJECT_BINARY_DIR}/src/util"
	"${PROJECT_BINARY_DIR}/include"
	"${PROJECT_SOURCE_DIR}/src/util"
	"${PROJECT_SOURCE_DIR}/include"
	${LIBGIT2_DEPENDENCY_INCLUDES}
	${LIBGIT2_SYSTEM_INCLUDES}
)

# The reftable library is not warning-free, so we disable turning warnings into
# errors.
if(MSVC)
        set_source_files_properties(block.c PROPERTIES COMPILE_FLAGS -WX-)
        set_source_files_properties(blocksource.c PROPERTIES COMPILE_FLAGS -WX-)
        set_source_files_properties(record.c PROPERTIES COMPILE_FLAGS -WX-)
        set_source_files_properties(stack.c PROPERTIES COMPILE_FLAGS -WX-)
        set_source_files_properties(table.c PROPERTIES COMPILE_FLAGS -WX-)
        set_source_files_properties(writer.c PROPERTIES COMPILE_FLAGS -WX-)
endif()
