#ifndef INCLUDE_git_refspec_h__
#define INCLUDE_git_refspec_h__

#include "git2/types.h"

/**
 * Get the source specifier
 *
 * @param refspec the refspec
 * @return the refspec's source specifier
 */
const char *git_refspec_src(const git_refspec *refspec);

/**
 * Get the destination specifier
 *
 * @param refspec the refspec
 * @return the refspec's destination specifier
 */
const char *git_refspec_dst(const git_refspec *refspec);

/**
 * Match a refspec's source descriptor with a reference name
 *
 * @param refspec the refspec
 * @param refname the name of the reference to check
 * @return GIT_SUCCESS on successful match; GIT_ENOMACH on match
 * failure or an error code on other failure
 */
int git_refspec_src_match(const git_refspec *refspec, const char *refname);

#endif
