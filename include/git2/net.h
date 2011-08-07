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
#ifndef INCLUDE_net_h__
#define INCLUDE_net_h__

#include "common.h"
#include "oid.h"
#include "types.h"

/**
 * @file git2/net.h
 * @brief Git networking declarations
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

#define GIT_DEFAULT_PORT "9418"

/*
 * We need this because we need to know whether we should call
 * git-upload-pack or git-receive-pack on the remote end when get_refs
 * gets called.
 */

#define GIT_DIR_FETCH 0
#define GIT_DIR_PUSH 1

/**
 * Remote head description, given out on `ls` calls.
 */
struct git_remote_head {
	int local:1; /* available locally */
	git_oid oid;
	git_oid loid;
	char *name;
};

/**
 * Array of remote heads
 */
struct git_headarray {
	unsigned int len;
	struct git_remote_head **heads;
};

/** @} */
GIT_END_DECL
#endif
