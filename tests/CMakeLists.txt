FIND_PACKAGE(PythonInterp)

IF(NOT PYTHONINTERP_FOUND)
  MESSAGE(FATAL_ERROR "Could not find a python interpeter, which is needed to build the tests. "
    "Make sure python is available, or pass -DBUILD_CLAR=OFF to skip building the tests")
ENDIF()

SET(CLAR_FIXTURES "${CMAKE_CURRENT_SOURCE_DIR}/resources/")
SET(CLAR_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
ADD_DEFINITIONS(-DCLAR_FIXTURE_PATH=\"${CLAR_FIXTURES}\")
ADD_DEFINITIONS(-DCLAR_TMPDIR=\"libgit2_tests\")
ADD_DEFINITIONS(-D_FILE_OFFSET_BITS=64)

INCLUDE_DIRECTORIES(${CLAR_PATH} ${libgit2_BINARY_DIR}/src)
FILE(GLOB_RECURSE SRC_TEST ${CLAR_PATH}/*/*.c ${CLAR_PATH}/*/*.h)
SET(SRC_CLAR "main.c" "clar_libgit2.c" "clar_libgit2_trace.c" "clar_libgit2_timer.c" "clar.c")

IF(MSVC_IDE)
	LIST(APPEND SRC_CLAR "precompiled.c")
ENDIF()

ADD_CUSTOM_COMMAND(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/clar.suite
	COMMAND ${PYTHON_EXECUTABLE} generate.py -o "${CMAKE_CURRENT_BINARY_DIR}" -f -xonline -xstress -xperf .
	DEPENDS ${SRC_TEST}
	WORKING_DIRECTORY ${CLAR_PATH}
)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

SET_SOURCE_FILES_PROPERTIES(
	${CLAR_PATH}/clar.c
	PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/clar.suite)

INCLUDE_DIRECTORIES(${LIBGIT2_INCLUDES})
INCLUDE_DIRECTORIES(SYSTEM ${LIBGIT2_SYSTEM_INCLUDES})

ADD_EXECUTABLE(libgit2_clar ${SRC_CLAR} ${SRC_TEST} ${LIBGIT2_OBJECTS})

SET_TARGET_PROPERTIES(libgit2_clar PROPERTIES C_STANDARD 90)
SET_TARGET_PROPERTIES(libgit2_clar PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${libgit2_BINARY_DIR})

IF (${CMAKE_VERSION} VERSION_LESS 2.8.12)
	# Already handled by a global INCLUDE_DIRECTORY()
ELSE()
	TARGET_INCLUDE_DIRECTORIES(libgit2_clar PRIVATE ../src PUBLIC ../include)
ENDIF()

TARGET_LINK_LIBRARIES(libgit2_clar ${LIBGIT2_LIBS})
IDE_SPLIT_SOURCES(libgit2_clar)

IF (MSVC_IDE)
	# Precompiled headers
	SET_TARGET_PROPERTIES(libgit2_clar PROPERTIES COMPILE_FLAGS "/Yuprecompiled.h /FIprecompiled.h")
	SET_SOURCE_FILES_PROPERTIES("precompiled.c" COMPILE_FLAGS "/Ycprecompiled.h")
ENDIF ()

ADD_TEST(offline   "${libgit2_BINARY_DIR}/libgit2_clar" -v -xonline)
ADD_TEST(online    "${libgit2_BINARY_DIR}/libgit2_clar" -v -sonline)
ADD_TEST(gitdaemon "${libgit2_BINARY_DIR}/libgit2_clar" -v -sonline::push)
ADD_TEST(ssh       "${libgit2_BINARY_DIR}/libgit2_clar" -v -sonline::push -sonline::clone::ssh_cert -sonline::clone::ssh_with_paths)
ADD_TEST(proxy     "${libgit2_BINARY_DIR}/libgit2_clar" -v -sonline::clone::proxy_credentials_in_url -sonline::clone::proxy_credentials_request)
