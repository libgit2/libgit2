IF(WIN32 AND WINHTTP)
	SET(GIT_WINHTTP 1 PARENT_SCOPE)

	# Since MinGW does not come with headers or an import library for winhttp,
	# we have to include a private header and generate our own import library
	IF(MINGW)
		ADD_SUBDIRECTORY("${libgit2_SOURCE_DIR}/deps/winhttp" "${libgit2_BINARY_DIR}/deps/winhttp")
		LIST(APPEND LIBGIT2_PC_LIBS -lrpcrt4 -lcrypt32 -lole32)
	ELSE()
		ADD_LIBRARY(WinHTTP INTERFACE)
		TARGET_LINK_LIBRARIES(WinHTTP INTERFACE winhttp rpcrt4 crypt32 ole32)
		LIST(APPEND LIBGIT2_PC_LIBS -lwinhttp -lrpcrt4 -lcrypt32 -lole32)
	ENDIF()
ELSE()
	ADD_LIBRARY(WinHTTP INTERFACE)
ENDIF()
