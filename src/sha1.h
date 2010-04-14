#ifndef INCLUDE_sha1_h__
#define INCLUDE_sha1_h__

#ifdef PPC_SHA1
# include "ppc/sha1.h"
#else
# include "block-sha1/sha1.h"
#endif

#endif /* INCLUDE_sha1_h__ */
