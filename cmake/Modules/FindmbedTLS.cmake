# - Try to find mbedTLS
# Once done this will define
#
#  MBEDTLS_ROOT_DIR - Set this variable to the root installation of mbedTLS
#
# Read-Only variables
#  MBEDTLS_FOUND - system has mbedTLS
#  MBEDTLS_INCLUDE_DIR - the mbedTLS include directory
#  MBEDTLS_LIBRARIES - Link these to use mbedTLS
#

FIND_PATH(MBEDTLS_ROOT_DIR NAMES include/mbedtls/version.h)

IF(MBEDTLS_INCLUDE_DIR AND MBEDTLS_LIBRARIES)
    # Already in cache, be silent
    SET(MBEDTLS_FIND_QUIETLY TRUE)
ENDIF()
FIND_PATH(MBEDTLS_INCLUDE_DIR
    NAMES mbedtls/ssl.h
    PATHS
        ${MBEDTLS_ROOT_DIR}/include
)
FIND_LIBRARY(mbedtls_lib
    NAMES mbedtls libmbedtls libmbedx509
    PATHS
        ${MBEDTLS_ROOT_DIR}/library
        ${MBEDTLS_ROOT_DIR}/build/library
)
FIND_LIBRARY(mbedx509_lib
    NAMES mbedx509 libmbedx509
    PATHS
        ${MBEDTLS_ROOT_DIR}/library
        ${MBEDTLS_ROOT_DIR}/build/library
)
FIND_LIBRARY(mbedcrypto_lib
    NAMES mbedcrypto libmbedcrypto
    PATHS
        ${MBEDTLS_ROOT_DIR}/library
        ${MBEDTLS_ROOT_DIR}/build/library
)

IF(MBEDTLS_INCLUDE_DIR AND mbedtls_lib AND mbedx509_lib AND mbedcrypto_lib)
     SET(MBEDTLS_FOUND TRUE)
ENDIF()

IF(MBEDTLS_FOUND)
    # split mbedTLS into -L and -l linker options, so we can set them for pkg-config
    GET_FILENAME_COMPONENT(mbedtls_path ${mbedtls_lib} PATH)
    GET_FILENAME_COMPONENT(mbedtls_name ${mbedtls_lib} NAME_WE)
    GET_FILENAME_COMPONENT(mbedx509_name ${mbedx509_lib} NAME_WE)
    GET_FILENAME_COMPONENT(mbedcrypto_name ${mbedcrypto_lib} NAME_WE)
    STRING(REGEX REPLACE "^lib" "" mbedtls_name ${mbedtls_name})
    STRING(REGEX REPLACE "^lib" "" mbedx509_name ${mbedx509_name})
    STRING(REGEX REPLACE "^lib" "" mbedcrypto_name ${mbedcrypto_name})
    SET(MBEDTLS_LIBRARIES "-L${mbedtls_path} -l${mbedtls_name} -l${mbedx509_name} -l${mbedcrypto_name}")

    IF(NOT MBEDTLS_FIND_QUIETLY)
        MESSAGE(STATUS "Found mbedTLS:")
        FILE(READ ${MBEDTLS_INCLUDE_DIR}/mbedtls/version.h MBEDTLSCONTENT)
        STRING(REGEX MATCH "MBEDTLS_VERSION_STRING +\"[0-9|.]+\"" MBEDTLSMATCH ${MBEDTLSCONTENT})
        IF (MBEDTLSMATCH)
            STRING(REGEX REPLACE "MBEDTLS_VERSION_STRING +\"([0-9|.]+)\"" "\\1" MBEDTLS_VERSION ${MBEDTLSMATCH})
            MESSAGE(STATUS "  version ${MBEDTLS_VERSION}")
        ENDIF(MBEDTLSMATCH)
        MESSAGE(STATUS "  TLS: ${mbedtls_lib}")
        MESSAGE(STATUS "  X509: ${mbedx509_lib}")
        MESSAGE(STATUS "  Crypto: ${mbedcrypto_lib}")
    ENDIF(NOT MBEDTLS_FIND_QUIETLY)
ELSE(MBEDTLS_FOUND)
    IF(MBEDTLS_FIND_REQUIRED)
        MESSAGE(FATAL_ERROR "Could not find mbedTLS")
    ENDIF(MBEDTLS_FIND_REQUIRED)
ENDIF(MBEDTLS_FOUND)

MARK_AS_ADVANCED(
    MBEDTLS_INCLUDE_DIR
    MBEDTLS_LIBRARIES
)
