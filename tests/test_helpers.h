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

#ifndef INCLUDE_test_helpers_h__
#define INCLUDE_test_helpers_h__

#include "test_lib.h"
#include <git/odb.h>


typedef struct object_data {
    unsigned char *bytes;  /* (compressed) bytes stored in object store */
    size_t        blen;    /* length of data in object store            */
    char          *id;     /* object id (sha1)                          */
    char          *type;   /* object type                               */
    char          *dir;    /* object store (fan-out) directory name     */
    char          *file;   /* object store filename                     */
    unsigned char *data;   /* (uncompressed) object data                */
    size_t        dlen;    /* length of (uncompressed) object data      */
} object_data;

extern int write_object_data(char *file, void *data, size_t len);

extern int write_object_files(const char *odb_dir, object_data *d);

extern int remove_object_files(const char *odb_dir, object_data *d);

extern int cmp_objects(git_obj *o, object_data *d);

#endif
/* INCLUDE_test_helpers_h__ */
