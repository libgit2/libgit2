# Specify regular expression implementation
find_package(PCRE)

set(OPTION_NAME "USE_REGEX")

# Fall back to the previous cmake configuration, "USE_BUNDLED_ZLIB"
if(NOT USE_REGEX AND REGEX_BACKEND)
	set(USE_REGEX "${REGEX_BACKEND}")
	set(OPTION_NAME "REGEX_BACKEND")
endif()

if(NOT USE_REGEX)
	check_symbol_exists(regcomp_l "regex.h;xlocale.h" HAVE_REGCOMP_L)

	if(HAVE_REGCOMP_L)
		# 'regcomp_l' has been explicitly marked unavailable on iOS_SDK
		if(CMAKE_SYSTEM_NAME MATCHES "iOS")
			set(USE_REGEX "regcomp")
		else()
			set(USE_REGEX "regcomp_l")
		endif()
	elseif(PCRE_FOUND)
		set(USE_REGEX "pcre")
	else()
		set(USE_REGEX "builtin")
	endif()
endif()

if(USE_REGEX STREQUAL "regcomp_l")
	add_feature_info("Regular expressions" ON "using system regcomp_l")
	set(GIT_REGEX_REGCOMP_L 1)
elseif(USE_REGEX STREQUAL "pcre2")
	find_package(PCRE2)

	if(NOT PCRE2_FOUND)
		MESSAGE(FATAL_ERROR "PCRE2 support was requested but not found")
	endif()

	add_feature_info("Regular expressions" ON "using system PCRE2")
	set(GIT_REGEX_PCRE2 1)

	list(APPEND LIBGIT2_SYSTEM_INCLUDES ${PCRE2_INCLUDE_DIRS})
	list(APPEND LIBGIT2_SYSTEM_LIBS ${PCRE2_LIBRARIES})
	list(APPEND LIBGIT2_PC_REQUIRES "libpcre2-8")
elseif(USE_REGEX STREQUAL "pcre")
	add_feature_info("Regular expressions" ON "using system PCRE")
	set(GIT_REGEX_PCRE 1)

	list(APPEND LIBGIT2_SYSTEM_INCLUDES ${PCRE_INCLUDE_DIRS})
	list(APPEND LIBGIT2_SYSTEM_LIBS ${PCRE_LIBRARIES})
	list(APPEND LIBGIT2_PC_REQUIRES "libpcre")
elseif(USE_REGEX STREQUAL "regcomp")
	add_feature_info("Regular expressions" ON "using system regcomp")
	set(GIT_REGEX_REGCOMP 1)
elseif(USE_REGEX STREQUAL "builtin")
	add_feature_info("Regular expressions" ON "using bundled implementation")
	set(GIT_REGEX_BUILTIN 1)

	add_subdirectory("${PROJECT_SOURCE_DIR}/deps/pcre" "${PROJECT_BINARY_DIR}/deps/pcre")
	list(APPEND LIBGIT2_DEPENDENCY_INCLUDES "${PROJECT_SOURCE_DIR}/deps/pcre")
	list(APPEND LIBGIT2_DEPENDENCY_OBJECTS $<TARGET_OBJECTS:pcre>)
else()
	message(FATAL_ERROR "unknown setting to ${OPTION_NAME}: ${USE_REGEX}")
endif()
