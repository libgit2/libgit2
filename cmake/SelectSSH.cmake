# Optional external dependency: libssh2
find_package(Libssh2 CONFIG REQUIRED)
if(Libssh2_FOUND)
	set(GIT_SSH 1)
	SET(GIT_SSH_MEMORY_CREDENTIALS 0)

	IF(EXISTS "${CMAKE_BINARY_DIR}/../sysroot/include/")
 	LIST(APPEND LIBGIT2_SYSTEM_INCLUDES "${CMAKE_BINARY_DIR}/../sysroot/include")
 	ELSE()
 	LIST(APPEND LIBGIT2_SYSTEM_INCLUDES "${CMAKE_BINARY_DIR}/../../sysroot/include")
 	endif()

	LIST(APPEND LIBGIT2_SYSTEM_LIBS  "Libssh2::libssh2")
	
 	if(WIN32)
 	LIST(APPEND LIBGIT2_SYSTEM_LIBS  "crypt32")
 	endif()
else()
	message(STATUS "LIBSSH2 not found. Set CMAKE_PREFIX_PATH if it is installed outside of the default search path.")
endif()

if(WIN32 AND EMBED_SSH_PATH)
	file(GLOB SSH_SRC "${EMBED_SSH_PATH}/src/*.c")
	list(SORT SSH_SRC)
	list(APPEND LIBGIT2_DEPENDENCY_OBJECTS ${SSH_SRC})

	list(APPEND LIBGIT2_DEPENDENCY_INCLUDES "${EMBED_SSH_PATH}/include")
	file(WRITE "${EMBED_SSH_PATH}/src/libssh2_config.h" "#define HAVE_WINCNG\n#define LIBSSH2_WINCNG\n#include \"../win32/libssh2_config.h\"")
	set(GIT_SSH 1)
endif()

add_feature_info(SSH GIT_SSH "SSH transport support")
