/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
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

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
	/* We don't support non-default attributes. */
	if (attr)
		return EINVAL;

	/* This is an auto-reset event. */
	*cond = CreateEventW(NULL, FALSE, FALSE, NULL);
	assert(*cond);

	/* If we can't create the event, claim that the reason was out-of-memory.
	 * The actual reason can be fetched with GetLastError(). */
	return *cond ? 0 : ENOMEM;
}

int pthread_cond_destroy(pthread_cond_t *cond)
{
	BOOL closed;

	if (!cond)
		return EINVAL;

	closed = CloseHandle(*cond);
	assert(closed);
	GIT_UNUSED(closed);

	*cond = NULL;
	return 0;
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	int error;
	DWORD wait_result;

	if (!cond || !mutex)
		return EINVAL;

	/* The caller must be holding the mutex. */
	error = pthread_mutex_unlock(mutex);

	if (error)
		return error;

	wait_result = WaitForSingleObject(*cond, INFINITE);
	assert(WAIT_OBJECT_0 == wait_result);
	GIT_UNUSED(wait_result);

	return pthread_mutex_lock(mutex);
}

int pthread_cond_signal(pthread_cond_t *cond)
{
	BOOL signaled;

	if (!cond)
		return EINVAL;

	signaled = SetEvent(*cond);
	assert(signaled);
	GIT_UNUSED(signaled);

	return 0;
}

/* pthread_cond_broadcast is not implemented because doing so with just Win32 events
 * is quite complicated, and no caller in libgit2 uses it yet. */

int pthread_num_processors_np(void)
{
	DWORD_PTR p, s;
	int n = 0;

	if (GetProcessAffinityMask(GetCurrentProcess(), &p, &s))
		for (; p; p >>= 1)
			n += p&1;

	return n ? n : 1;
}

