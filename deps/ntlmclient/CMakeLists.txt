add_library(ntlmclient STATIC
	ntlm.c
	ntlm.h
	util.c
	util.h
)

target_compile_definitions(ntlmclient PRIVATE NTLM_STATIC=1)
target_include_directories(ntlmclient PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

disable_warnings(implicit-fallthrough)

if(USE_ICONV)
	target_compile_definitions(ntlmclient PRIVATE UNICODE_ICONV=1)
	target_sources(ntlmclient PRIVATE unicode_iconv.c unicode_iconv.h)
else()
	target_compile_definitions(ntlmclient PRIVATE UNICODE_BUILTIN=1)
	target_sources(ntlmclient PRIVATE unicode_builtin.c unicode_builtin.h)
endif()

if(USE_HTTPS STREQUAL "SecureTransport")
	target_compile_definitions(ntlmclient PRIVATE CRYPT_COMMONCRYPTO)
	target_sources(ntlmclient PRIVATE crypt_commoncrypto.c crypt_commoncrypto.h)
	# CC_MD4 has been deprecated in macOS 10.15.
	set_source_files_properties(crypt_commoncrypto.c COMPILE_FLAGS "-Wno-deprecated")
elseif(USE_HTTPS STREQUAL "OpenSSL")
	target_compile_definitions(ntlmclient PRIVATE CRYPT_OPENSSL OPENSSL_API_COMPAT=0x10100000L)
	target_link_libraries(ntlmclient PRIVATE OpenSSL::SSL)
	target_sources(ntlmclient PRIVATE crypt_openssl.c crypt_openssl.h)
elseif(USE_HTTPS STREQUAL "OpenSSL-Dynamic")
	target_compile_definitions(ntlmclient PRIVATE CRYPT_OPENSSL CRYPT_OPENSSL_DYNAMIC OPENSSL_API_COMPAT=0x10100000L)
	target_link_libraries(ntlmclient PRIVATE OpenSSL::SSL)
	target_sources(ntlmclient PRIVATE crypt_openssl.c crypt_openssl.h)
elseif(USE_HTTPS STREQUAL "mbedTLS")
	target_compile_definitions(ntlmclient PRIVATE CRYPT_MBEDTLS)
	target_include_directories(ntlmclient PRIVATE ${MBEDTLS_INCLUDE_DIR})
	target_sources(ntlmclient PRIVATE crypt_mbedtls.c crypt_mbedtls.h crypt_builtin_md4.c)
else()
	message(FATAL_ERROR "Unable to use libgit2's HTTPS backend (${USE_HTTPS}) for NTLM crypto")
endif()
