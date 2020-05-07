#ifndef INCLUDE_features_h__
#define INCLUDE_features_h__

//#define GIT_DEBUG_POOL 1
//#define GIT_TRACE 1
#define GIT_THREADS 1
//#define GIT_MSVC_CRTDBG 1

#define GIT_ARCH_64 1
//#define GIT_ARCH_32 1

#define GIT_USE_ICONV 1
//#define GIT_USE_NSEC 1
//#define GIT_USE_STAT_MTIM 1
//#define GIT_USE_STAT_MTIMESPEC 1
#define GIT_USE_STAT_MTIME_NSEC 1
//#define GIT_USE_FUTIMENS 1

#define GIT_REGEX_REGCOMP_L
//#define GIT_REGEX_REGCOMP
//#define GIT_REGEX_PCRE
//#define GIT_REGEX_PCRE2
//#define GIT_REGEX_BUILTIN 1

//#define GIT_SSH 1
//#define GIT_SSH_MEMORY_CREDENTIALS 1

//#define GIT_NTLM 1
//#define GIT_GSSAPI 1
//#define GIT_GSSFRAMEWORK 1

//#define GIT_WINHTTP 1
#define GIT_HTTPS 1
//#define GIT_OPENSSL 1
#define GIT_SECURE_TRANSPORT 1
//#define GIT_MBEDTLS 1

//#define GIT_SHA1_COLLISIONDETECT 1
//#define GIT_SHA1_WIN32 1
#define GIT_SHA1_COMMON_CRYPTO 1
//#define GIT_SHA1_OPENSSL 1
//#define GIT_SHA1_MBEDTLS 1

#endif
