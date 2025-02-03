# The main libgit2 source tree: this CMakeLists.txt identifies platform
# support and includes the subprojects that make up core libgit2 support.

#
# Optional build configuration settings
#

if(DEPRECATE_HARD)
        add_definitions(-DGIT_DEPRECATE_HARD)
endif()

if(DEBUG_LEAK_CHECKER STREQUAL "valgrind")
	add_definitions(-DVALGRIND)
endif()

#
# Optional debugging functionality
#

if(DEBUG_POOL)
	set(GIT_DEBUG_POOL 1)
endif()
add_feature_info("Debug pool" GIT_DEBUG_POOL "debug-mode struct pool allocators")

if(DEBUG_STRICT_ALLOC)
	set(GIT_DEBUG_STRICT_ALLOC 1)
endif()
add_feature_info("Debug alloc" GIT_DEBUG_STRICT_ALLOC "debug-mode strict allocators")

if(DEBUG_STRICT_OPEN)
	set(GIT_DEBUG_STRICT_OPEN 1)
endif()
add_feature_info("Debug open" GIT_DEBUG_STRICT_OPEN "strict path validation in open")

#
# Optional feature enablement
#

include(SelectThreads)
include(SelectNsec)
include(SelectHTTPSBackend)
include(SelectHashes)
include(SelectHTTPParser)
include(SelectRegex)
include(SelectXdiff)
include(SelectSSH)
include(SelectCompression)
include(SelectI18n)
include(SelectAuthNTLM)
include(SelectAuthNegotiate)

#
# Platform support
#

# futimes/futimens

if(HAVE_FUTIMENS)
	set(GIT_FUTIMENS 1)
endif()

# qsort

# old-style FreeBSD qsort_r() has the 'context' parameter as the first argument
# of the comparison function:
check_prototype_definition_safe(qsort_r
	"void (qsort_r)(void *base, size_t nmemb, size_t size, void *context, int (*compar)(void *, const void *, const void *))"
	"" "stdlib.h" GIT_QSORT_BSD)

# GNU or POSIX qsort_r() has the 'context' parameter as the last argument of the
# comparison function:
check_prototype_definition_safe(qsort_r
	"void (qsort_r)(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *, void *), void *context)"
	"" "stdlib.h" GIT_QSORT_GNU)

# C11 qsort_s() has the 'context' parameter as the last argument of the
# comparison function, and returns an error status:
check_prototype_definition_safe(qsort_s
	"errno_t (qsort_s)(void *base, rsize_t nmemb, rsize_t size, int (*compar)(const void *, const void *, void *), void *context)"
	"0" "stdlib.h" GIT_QSORT_C11)

# MSC qsort_s() has the 'context' parameter as the first argument of the
# comparison function, and as the last argument of qsort_s():
check_prototype_definition_safe(qsort_s
	"void (qsort_s)(void *base, size_t num, size_t width, int (*compare )(void *, const void *, const void *), void *context)"
	"" "stdlib.h" GIT_QSORT_MSC)

# random / entropy data

check_symbol_exists(getentropy unistd.h GIT_RAND_GETENTROPY)
check_symbol_exists(getloadavg stdlib.h GIT_RAND_GETLOADAVG)

# poll

if(WIN32)
	set(GIT_IO_WSAPOLL 1)
else()
	check_symbol_exists(poll poll.h GIT_IO_POLL)
	check_symbol_exists(select sys/select.h GIT_IO_SELECT)
endif()

# determine architecture of the machine

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(GIT_ARCH_64 1)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
	set(GIT_ARCH_32 1)
elseif(CMAKE_SIZEOF_VOID_P)
	message(FATAL_ERROR "Unsupported architecture (pointer size is ${CMAKE_SIZEOF_VOID_P} bytes)")
else()
	message(FATAL_ERROR "Unsupported architecture (CMAKE_SIZEOF_VOID_P is unset)")
endif()

# realtime support

check_library_exists(rt clock_gettime "time.h" NEED_LIBRT)

if(NEED_LIBRT AND NOT CMAKE_SYSTEM_NAME MATCHES "iOS")
	list(APPEND LIBGIT2_SYSTEM_LIBS rt)
	list(APPEND LIBGIT2_PC_LIBS "-lrt")
endif()

# platform libraries

if(WIN32)
	list(APPEND LIBGIT2_SYSTEM_LIBS "ws2_32" "secur32")
	list(APPEND LIBGIT2_PC_LIBS "-lws2_32" "-lsecur32")
endif()

if(CMAKE_SYSTEM_NAME MATCHES "(Solaris|SunOS)")
	list(APPEND LIBGIT2_SYSTEM_LIBS socket nsl)
	list(APPEND LIBGIT2_PC_LIBS "-lsocket" "-lnsl")
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Haiku")
	list(APPEND LIBGIT2_SYSTEM_LIBS gnu network)
	list(APPEND LIBGIT2_PC_LIBS "-lgnu -lnetwork")
endif()

if(AMIGA)
	add_definitions(-DNO_ADDRINFO -DNO_READDIR_R -DNO_MMAP)
endif()

#
# Set build time information
#

set(GIT_BUILD_CPU "${CMAKE_SYSTEM_PROCESSOR}")
execute_process(COMMAND git rev-parse HEAD
	OUTPUT_VARIABLE GIT_BUILD_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE)

#
# Include child projects
#

add_subdirectory(libgit2)
add_subdirectory(util)

if(BUILD_CLI AND NOT CMAKE_SYSTEM_NAME MATCHES "iOS")
	add_subdirectory(cli)
endif()

# re-export these to the root so that peer projects (tests, fuzzers,
# examples) can use them
set(LIBGIT2_INCLUDES ${LIBGIT2_INCLUDES} PARENT_SCOPE)
set(LIBGIT2_OBJECTS ${LIBGIT2_OBJECTS} PARENT_SCOPE)
set(LIBGIT2_DEPENDENCY_INCLUDES ${LIBGIT2_DEPENDENCY_INCLUDES} PARENT_SCOPE)
set(LIBGIT2_DEPENDENCY_OBJECTS ${LIBGIT2_DEPENDENCY_OBJECTS} PARENT_SCOPE)
set(LIBGIT2_SYSTEM_INCLUDES ${LIBGIT2_SYSTEM_INCLUDES} PARENT_SCOPE)
set(LIBGIT2_SYSTEM_LIBS ${LIBGIT2_SYSTEM_LIBS} PARENT_SCOPE)
