#ifndef INCLUDE_msvc_compat__
#define INCLUDE_msvc_compat__

#if defined(_MSC_VER)

/* access() mode parameter #defines   */
# define F_OK 0  /* existence  check */
# define W_OK 2  /* write mode check */
# define R_OK 4  /* read  mode check */

/* use a 64-bit file offset type */
typedef __int64 off64_t;
# define off_t off64_t
# define lseek _lseeki64
# define stat _stat64
# define fstat _fstat64

/* stat: file mode type testing macros */
# define S_ISDIR(m)   (((m) & _S_IFMT) == _S_IFDIR)
# define S_ISREG(m)   (((m) & _S_IFMT) == _S_IFREG)
# define S_ISFIFO(m)  (((m) & _S_IFMT) == _S_IFIFO)

/* add some missing <stdint.h> typedef's */
typedef signed char int8_t;
typedef unsigned char uint8_t;

typedef short int16_t;
typedef unsigned short uint16_t;

typedef long int32_t;
typedef unsigned long uint32_t;

typedef long long int64_t;
typedef unsigned long long uint64_t;

typedef long long intmax_t;
typedef unsigned long long uintmax_t;

#endif

#endif /* INCLUDE_msvc_compat__ */
