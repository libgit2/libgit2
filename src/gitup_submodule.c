#include "submodule.h"
#include "git2/gitup_submodule.h"

#include "git2/types.h"
#include "vector.h"
#include "posix.h"
#include "config.h"
#include "repository.h"
#include "tree.h"
#include "index.h"

/**
 * Retains a submodule
 *
 * @param submodule Submodule object
 */
// GIT_EXTERN(void) git_submodule_retain(git_submodule *submodule);
void gitup_submodule_dup(git_submodule *sm)
{
    git_submodule_dup(&sm, sm);
}

