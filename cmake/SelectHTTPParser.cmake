# Optional external dependency: http-parser
if(USE_HTTP_PARSER STREQUAL "builtin")
    message(STATUS "support for bundled (legacy) http-parser explicitly requested")

	add_subdirectory("${PROJECT_SOURCE_DIR}/deps/http-parser" "${PROJECT_BINARY_DIR}/deps/http-parser")
	list(APPEND LIBGIT2_DEPENDENCY_INCLUDES "${PROJECT_SOURCE_DIR}/deps/http-parser")
	list(APPEND LIBGIT2_DEPENDENCY_OBJECTS "$<TARGET_OBJECTS:http-parser>")
	add_feature_info(http-parser ON "http-parser support (bundled)")
else()
    # By default, try to use system LLHTTP. Fall back to
    # system http-parser, and even to bundled http-parser
    # as a last resort.
    find_package(LLHTTP)

	if(LLHTTP_FOUND AND LLHTTP_VERSION_MAJOR EQUAL 9)
	    add_compile_definitions(USE_LLHTTP)
		list(APPEND LIBGIT2_SYSTEM_INCLUDES ${LLHTTP_INCLUDE_DIRS})
		list(APPEND LIBGIT2_SYSTEM_LIBS ${LLHTTP_LIBRARIES})
		list(APPEND LIBGIT2_PC_LIBS "-lllhttp")
		add_feature_info(llhttp ON "llhttp support (system)")
	else()
        message(STATUS "llhttp support was requested but not found; checking (legacy) http-parser support")
        find_package(HTTPParser)

        if(HTTP_PARSER_FOUND AND HTTP_PARSER_VERSION_MAJOR EQUAL 2)
            list(APPEND LIBGIT2_SYSTEM_INCLUDES ${HTTP_PARSER_INCLUDE_DIRS})
            list(APPEND LIBGIT2_SYSTEM_LIBS ${HTTP_PARSER_LIBRARIES})
            list(APPEND LIBGIT2_PC_LIBS "-lhttp_parser")
            add_feature_info(http-parser ON "http-parser support (system)")
        else()
            message(STATUS "neither llhttp nor http-parser support was found; proceeding with bundled (legacy) http-parser")

            add_subdirectory("${PROJECT_SOURCE_DIR}/deps/http-parser" "${PROJECT_BINARY_DIR}/deps/http-parser")
            list(APPEND LIBGIT2_DEPENDENCY_INCLUDES "${PROJECT_SOURCE_DIR}/deps/http-parser")
            list(APPEND LIBGIT2_DEPENDENCY_OBJECTS "$<TARGET_OBJECTS:http-parser>")
            add_feature_info(http-parser ON "http-parser support (bundled)")
        endif()
    endif()
endif()
