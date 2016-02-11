/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#nodef GITERR_CHECK_ALLOC(ptr) if (ptr == NULL) { __coverity_panic__(); }
