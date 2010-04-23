#ifndef INCLUDE_mingw_compat__
#define INCLUDE_mingw_compat__

#if defined(__MINGW32__)

/* use a 64-bit file offset type */
# define off_t off64_t
# define lseek _lseeki64
# define stat _stati64
# define fstat _fstati64

#endif

#endif /* INCLUDE_mingw_compat__ */
