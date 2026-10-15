include(SanitizeInput)

# We try to find any packages our backends might use
find_package(OpenSSL)
find_package(mbedTLS)

if(CMAKE_SYSTEM_NAME MATCHES "Darwin" OR CMAKE_SYSTEM_NAME MATCHES "iOS")
	find_package(Security)
	find_package(CoreFoundation)
endif()

if(USE_HTTPS STREQUAL "")
	set(USE_HTTPS ON)
endif()

sanitizeinput(USE_HTTPS)

if(USE_HTTPS)
	# Auto-select TLS backend
	if(USE_HTTPS STREQUAL ON)
		if(SECURITY_FOUND)
			if(SECURITY_HAS_SSLCREATECONTEXT)
				set(USE_HTTPS "securetransport")
			else()
				message(STATUS "Security framework is too old, falling back to OpenSSL")
				set(USE_HTTPS "OpenSSL")
			endif()
		elseif(WIN32)
			set(USE_HTTPS "winhttp")
		elseif(OPENSSL_FOUND)
			set(USE_HTTPS "openssl")
		elseif(MBEDTLS_FOUND)
			set(USE_HTTPS "mbedtls")
		else()
			message(FATAL_ERROR "Unable to autodetect a usable HTTPS backend."
				"Please pass the backend name explicitly (-DUSE_HTTPS=backend)")
		endif()
	endif()

	# Check that we can find what's required for the selected backend
	if(USE_HTTPS STREQUAL "securetransport")
		if(NOT COREFOUNDATION_FOUND)
			message(FATAL_ERROR "Cannot use SecureTransport backend, CoreFoundation.framework not found")
		endif()
		if(NOT SECURITY_FOUND)
			message(FATAL_ERROR "Cannot use SecureTransport backend, Security.framework not found")
		endif()
		if(NOT SECURITY_HAS_SSLCREATECONTEXT)
			message(FATAL_ERROR "Cannot use SecureTransport backend, SSLCreateContext not supported")
		endif()

		list(APPEND LIBGIT2_SYSTEM_INCLUDES ${SECURITY_INCLUDE_DIR})
		list(APPEND LIBGIT2_SYSTEM_LIBS ${COREFOUNDATION_LDFLAGS} ${SECURITY_LDFLAGS})
		list(APPEND LIBGIT2_PC_LIBS ${COREFOUNDATION_LDFLAGS} ${SECURITY_LDFLAGS})

		set(GIT_HTTPS_SECURETRANSPORT 1)
		add_feature_info("HTTPS" ON "using SecureTransport")
	elseif(USE_HTTPS STREQUAL "openssl")
		if(NOT OPENSSL_FOUND)
			message(FATAL_ERROR "Asked for OpenSSL TLS backend, but it wasn't found")
		endif()

		list(APPEND LIBGIT2_SYSTEM_INCLUDES ${OPENSSL_INCLUDE_DIR})
		list(APPEND LIBGIT2_SYSTEM_LIBS ${OPENSSL_LIBRARIES})

		# Static OpenSSL (lib crypto.a) requires libdl, include it explicitly
		if(LINK_WITH_STATIC_LIBRARIES STREQUAL ON)
			list(APPEND LIBGIT2_SYSTEM_LIBS ${CMAKE_DL_LIBS})
		endif()

		list(APPEND LIBGIT2_PC_LIBS ${OPENSSL_LDFLAGS})
		list(APPEND LIBGIT2_PC_REQUIRES "openssl")

		set(GIT_HTTPS_OPENSSL 1)
		add_feature_info("HTTPS" ON "using OpenSSL")
	elseif(USE_HTTPS STREQUAL "mbedtls")
		if(NOT MBEDTLS_FOUND)
			message(FATAL_ERROR "Asked for mbedTLS backend, but it wasn't found")
		endif()

		if(NOT CERT_LOCATION)
			message(STATUS "Auto-detecting default certificates location")
			if(EXISTS "/usr/local/opt/openssl/bin/openssl")
				# Check for an Homebrew installation
				set(OPENSSL_CMD "/usr/local/opt/openssl/bin/openssl")
			else()
				set(OPENSSL_CMD "openssl")
			endif()
			execute_process(COMMAND ${OPENSSL_CMD} version -d OUTPUT_VARIABLE OPENSSL_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
			if(OPENSSL_DIR)
				string(REGEX REPLACE "^OPENSSLDIR: \"(.*)\"$" "\\1/" OPENSSL_DIR ${OPENSSL_DIR})

				set(OPENSSL_CA_LOCATIONS
					"ca-bundle.pem"             # OpenSUSE Leap 42.1
					"cert.pem"                  # Ubuntu 14.04, FreeBSD
					"certs/ca-certificates.crt" # Ubuntu 16.04
					"certs/ca.pem"              # Debian 7
				)
				foreach(SUFFIX IN LISTS OPENSSL_CA_LOCATIONS)
					set(LOC "${OPENSSL_DIR}${SUFFIX}")
					if(NOT CERT_LOCATION AND EXISTS "${OPENSSL_DIR}${SUFFIX}")
						set(CERT_LOCATION ${LOC})
					endif()
				endforeach()
			else()
				message(FATAL_ERROR "Unable to find OpenSSL executable. Please provide default certificate location via CERT_LOCATION")
			endif()
		endif()

		if(CERT_LOCATION)
			if(NOT EXISTS ${CERT_LOCATION})
				message(FATAL_ERROR "cannot use CERT_LOCATION=${CERT_LOCATION} as it doesn't exist")
			endif()
			add_definitions(-DGIT_DEFAULT_CERT_LOCATION="${CERT_LOCATION}")
		endif()

		list(APPEND LIBGIT2_SYSTEM_INCLUDES ${MBEDTLS_INCLUDE_DIR})
		list(APPEND LIBGIT2_SYSTEM_LIBS ${MBEDTLS_LIBRARIES})
		# mbedTLS has no pkgconfig file, hence we can't require it
		# https://github.com/ARMmbed/mbedtls/issues/228
		# For now, pass its link flags as our own
		list(APPEND LIBGIT2_PC_LIBS ${MBEDTLS_LIBRARIES})

		set(GIT_HTTPS_MBEDTLS 1)

		if(CERT_LOCATION)
			add_feature_info("HTTPS" ON "using mbedTLS (certificate location: ${CERT_LOCATION})")
		else()
			add_feature_info("HTTPS" ON "using mbedTLS")
		endif()
	elseif(USE_HTTPS STREQUAL "schannel")
		list(APPEND LIBGIT2_SYSTEM_LIBS "rpcrt4" "crypt32" "ole32")
		list(APPEND LIBGIT2_PC_LIBS "-lrpcrt4" "-lcrypt32" "-lole32")

		set(GIT_HTTPS_SCHANNEL 1)
		add_feature_info("HTTPS" ON "using Schannel")
	elseif(USE_HTTPS STREQUAL "winhttp")
		# Since MinGW does not come with headers or an import library for winhttp,
		# we have to include a private header and generate our own import library
		if(MINGW)
			add_subdirectory("${PROJECT_SOURCE_DIR}/deps/winhttp" "${PROJECT_BINARY_DIR}/deps/winhttp")
			list(APPEND LIBGIT2_SYSTEM_LIBS winhttp)
			list(APPEND LIBGIT2_DEPENDENCY_INCLUDES "${PROJECT_SOURCE_DIR}/deps/winhttp")
		else()
			list(APPEND LIBGIT2_SYSTEM_LIBS "winhttp")
			list(APPEND LIBGIT2_PC_LIBS "-lwinhttp")
		endif()

		list(APPEND LIBGIT2_SYSTEM_LIBS "rpcrt4" "crypt32" "ole32")
		list(APPEND LIBGIT2_PC_LIBS "-lrpcrt4" "-lcrypt32" "-lole32")

		set(GIT_HTTPS_WINHTTP 1)
		add_feature_info("HTTPS" ON "using WinHTTP")
	elseif(USE_HTTPS STREQUAL "openssl-dynamic")
		list(APPEND LIBGIT2_SYSTEM_LIBS dl)

		set(GIT_HTTPS_OPENSSL_DYNAMIC 1)
		add_feature_info("HTTPS" ON "using OpenSSL-Dynamic")
	else()
		message(FATAL_ERROR "unknown HTTPS backend: ${USE_HTTPS}")
	endif()

	set(GIT_HTTPS 1)
else()
	set(GIT_HTTPS 0)
	add_feature_info("HTTPS" NO "HTTPS support is disabled")
endif()
