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

#include "pthread.h"

int pthread_create(pthread_t *GIT_RESTRICT thread,
                   const pthread_attr_t *GIT_RESTRICT GIT_UNUSED(attr),
                   void *(*start_routine)(void*), void *GIT_RESTRICT arg)
{
	GIT_UNUSED_ARG(attr);
	*thread = (pthread_t) CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)start_routine, arg, 0, NULL);
	return *thread ? GIT_SUCCESS : GIT_EOSERR;
}

int pthread_join(pthread_t thread, void **value_ptr)
{
    int ret;
    ret = WaitForSingleObject(thread, INFINITE);
    if (ret && value_ptr)
        GetExitCodeThread(thread, (void*) value_ptr);
    return -(!!ret);
}

int pthread_mutex_init(pthread_mutex_t *GIT_RESTRICT mutex,
                       const pthread_mutexattr_t *GIT_RESTRICT GIT_UNUSED(mutexattr))
{
	GIT_UNUSED_ARG(mutexattr);
    InitializeCriticalSection(mutex);
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    int ret;
    ret = CloseHandle(mutex);
    return -(!ret);
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    EnterCriticalSection(mutex);
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    LeaveCriticalSection(mutex);
    return 0;
}

int pthread_num_processors_np(void)
{
    DWORD_PTR p, s;
    int n = 0;

    if (GetProcessAffinityMask(GetCurrentProcess(), &p, &s))
        for (; p; p >>= 1)
            n += p&1;

    return n ? n : 1;
}

