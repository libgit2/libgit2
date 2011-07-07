/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef INCLUDE_git_git_h__
#define INCLUDE_git_git_h__

#define LIBGIT2_VERSION "0.14.0"
#define LIBGIT2_VER_MAJOR 0
#define LIBGIT2_VER_MINOR 14
#define LIBGIT2_VER_REVISION 0

#include "git2/common.h"
#include "git2/errors.h"
#include "git2/zlib.h"

#include "git2/types.h"

#include "git2/oid.h"
#include "git2/signature.h"
#include "git2/odb.h"

#include "git2/repository.h"
#include "git2/revwalk.h"
#include "git2/refs.h"
#include "git2/reflog.h"

#include "git2/object.h"
#include "git2/blob.h"
#include "git2/commit.h"
#include "git2/tag.h"
#include "git2/tree.h"

#include "git2/index.h"
#include "git2/config.h"
#include "git2/remote.h"

#include "git2/refspec.h"
#include "git2/net.h"
#include "git2/transport.h"
#include "git2/status.h"
#include "git2/indexer.h"

#endif
