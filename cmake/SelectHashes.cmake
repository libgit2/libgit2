# Select a hash backend

include(SanitizeBool)

# USE_SHA1=CollisionDetection(ON)/HTTPS/Generic/OFF
sanitizebool(USE_SHA1)

if(USE_SHA1 STREQUAL ON)
	SET(USE_SHA1 "CollisionDetection")
elseif(USE_SHA1 STREQUAL "HTTPS")
	if(USE_HTTPS STREQUAL "SecureTransport")
		set(USE_SHA1 "CommonCrypto")
	elseif(USE_HTTPS STREQUAL "WinHTTP")
		set(USE_SHA1 "Win32")
	elseif(USE_HTTPS)
		set(USE_SHA1 ${USE_HTTPS})
	else()
		set(USE_SHA1 "CollisionDetection")
	endif()
endif()

if(USE_SHA1 STREQUAL "CollisionDetection")
	set(GIT_SHA1_COLLISIONDETECT 1)
elseif(USE_SHA1 STREQUAL "OpenSSL")
	# OPENSSL_FOUND should already be set, we're checking USE_HTTPS

	set(GIT_SHA1_OPENSSL 1)
	if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
		list(APPEND LIBGIT2_PC_LIBS "-lssl")
	else()
		list(APPEND LIBGIT2_PC_REQUIRES "openssl")
	endif()
elseif(USE_SHA1 STREQUAL "CommonCrypto")
	set(GIT_SHA1_COMMON_CRYPTO 1)
elseif(USE_SHA1 STREQUAL "mbedTLS")
	set(GIT_SHA1_MBEDTLS 1)
	list(APPEND LIBGIT2_SYSTEM_INCLUDES ${MBEDTLS_INCLUDE_DIR})
	list(APPEND LIBGIT2_SYSTEM_LIBS ${MBEDTLS_LIBRARIES})
	# mbedTLS has no pkgconfig file, hence we can't require it
	# https://github.com/ARMmbed/mbedtls/issues/228
	# For now, pass its link flags as our own
	list(APPEND LIBGIT2_PC_LIBS ${MBEDTLS_LIBRARIES})
elseif(USE_SHA1 STREQUAL "Win32")
	set(GIT_SHA1_WIN32 1)
elseif(NOT (USE_SHA1 STREQUAL "Generic"))
	message(FATAL_ERROR "Asked for unknown SHA1 backend: ${USE_SHA1}")
endif()

add_feature_info(SHA ON "using ${USE_SHA1}")
