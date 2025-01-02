# Optional external dependency: xdiff

include(SanitizeInput)

sanitizeinput(USE_XDIFF)

if(USE_XDIFF STREQUAL "system")
	message(FATAL_ERROR "external/system xdiff is not yet supported")
elseif(USE_XDIFF STREQUAL "builtin" OR USE_XDIFF STREQUAL "")
	add_subdirectory("${PROJECT_SOURCE_DIR}/deps/xdiff" "${PROJECT_BINARY_DIR}/deps/xdiff")
	list(APPEND LIBGIT2_DEPENDENCY_INCLUDES "${PROJECT_SOURCE_DIR}/deps/xdiff")
	list(APPEND LIBGIT2_DEPENDENCY_OBJECTS "$<TARGET_OBJECTS:xdiff>")
	add_feature_info("Xdiff" ON "using bundled provider")
else()
	message(FATAL_ERROR "asked for unknown Xdiff backend: ${USE_XDIFF}")
endif()
