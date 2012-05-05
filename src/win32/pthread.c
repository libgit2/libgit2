/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "pthread.h"

int pthread_create(
	pthread_t *GIT_RESTRICT thread,
	const pthread_attr_t *GIT_RESTRICT attr,
	void *(*start_routine)(void*),
	void *GIT_RESTRICT arg)
{
	GIT_UNUSED(attr);
	*thread = (pthread_t) CreateThread(
		NULL, 0, (LPTHREAD_START_ROUTINE)start_routine, arg, 0, NULL);
	return *thread ? 0 : -1;
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
						const pthread_mutexattr_t *GIT_RESTRICT mutexattr)
{
	GIT_UNUSED(mutexattr);
	InitializeCriticalSection(mutex);
	return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	DeleteCriticalSection(mutex);
	return 0;
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

