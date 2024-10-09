# Optional external dependency: xdiff
add_library(libgit2_xdiff INTERFACE)

if(USE_XDIFF STREQUAL "system")
	message(FATAL_ERROR "external/system xdiff is not yet supported")
else()
	add_subdirectory("${PROJECT_SOURCE_DIR}/deps/xdiff" "${PROJECT_BINARY_DIR}/deps/xdiff")
	target_link_libraries(libgit2_xdiff INTERFACE xdiff)
	add_feature_info(xdiff ON "xdiff support (bundled)")
endif()
