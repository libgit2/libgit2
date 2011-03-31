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
 *
 *  Original code by Ramiro Polla (Public Domain)
 */

#ifndef GIT_PTHREAD_H
#define GIT_PTHREAD_H

#include "../common.h"

#if defined (_MSC_VER)
#	define GIT_RESTRICT __restrict
#else
#	define GIT_RESTRICT __restrict__
#endif

typedef int pthread_mutexattr_t;
typedef int pthread_condattr_t;
typedef int pthread_attr_t;
typedef CRITICAL_SECTION pthread_mutex_t;
typedef HANDLE pthread_t;

#define PTHREAD_MUTEX_INITIALIZER {(void*)-1};

int pthread_create(pthread_t *GIT_RESTRICT,
                   const pthread_attr_t *GIT_RESTRICT,
                   void *(*start_routine)(void*), void *__restrict);

int pthread_join(pthread_t, void **);

int pthread_mutex_init(pthread_mutex_t *GIT_RESTRICT, const pthread_mutexattr_t *GIT_RESTRICT);
int pthread_mutex_destroy(pthread_mutex_t *);
int pthread_mutex_lock(pthread_mutex_t *);
int pthread_mutex_unlock(pthread_mutex_t *);

int pthread_num_processors_np(void);

#endif
