# Optional external dependency: http-parser

IF(USE_HTTP_PARSER STREQUAL "system")
	# Find the header and library
	FIND_PATH(HTTP_PARSER_INCLUDE_DIR NAMES http_parser.h)
	FIND_LIBRARY(HTTP_PARSER_LIBRARY NAMES http_parser libhttp_parser)

	# Handle the QUIETLY and REQUIRED arguments and set HTTP_PARSER_FOUND
	# to TRUE if all listed variables are TRUE
	INCLUDE(FindPackageHandleStandardArgs)
	FIND_PACKAGE_HANDLE_STANDARD_ARGS(HTTP_Parser REQUIRED_VARS HTTP_PARSER_INCLUDE_DIR HTTP_PARSER_LIBRARY)

	# Hide advanced variables
	MARK_AS_ADVANCED(HTTP_PARSER_INCLUDE_DIR HTTP_PARSER_LIBRARY)

	# Set standard variables
	IF (HTTP_PARSER_FOUND)
		ADD_LIBRARY(HttpParser INTERFACE)
		TARGET_LINK_LIBRARIES(HttpParser ${HTTP_PARSER_LIBRARY})
		TARGET_INCLUDE_DIRECTORIES(HttpParser ${HTTP_PARSER_INCLUDE_DIR})
	ENDIF()

	ADD_FEATURE_INFO(http-parser ON "using system http-parser")
ELSE()
	MESSAGE(STATUS "http-parser version 2 was not found or disabled; using bundled 3rd-party sources.")
	ADD_SUBDIRECTORY("${libgit2_SOURCE_DIR}/deps/http-parser" "${libgit2_BINARY_DIR}/deps/http-parser")

	ADD_FEATURE_INFO(http-parser ON "using bundled http-parser")
ENDIF()
