/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_thread_utils_h__
#define INCLUDE_thread_utils_h__

#if defined(GIT_THREADS)

#if defined(__clang__)

# if (__clang_major__ < 3 || (__clang_major__ == 3 && __clang_minor__ < 1))
#  error Atomic primitives do not exist on this version of clang; configure libgit2 with -DTHREADSAFE=OFF
# else
#  define GIT_BUILTIN_ATOMIC
# endif

#elif defined(__GNUC__)

# if (__GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 1))
#  error Atomic primitives do not exist on this version of gcc; configure libgit2 with -DTHREADSAFE=OFF
# elif (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7))
#  define GIT_BUILTIN_ATOMIC
# else
#  define GIT_BUILTIN_SYNC
# endif

#endif

#endif /* GIT_THREADS */

/* Common operations even if threading has been disabled */
typedef struct {
#if defined(GIT_WIN32)
	volatile long val;
#else
	volatile int val;
#endif
} git_atomic;

#ifdef GIT_ARCH_64

typedef struct {
#if defined(GIT_WIN32)
	volatile __int64 val;
#else
	volatile int64_t val;
#endif
} git_atomic64;

typedef git_atomic64 git_atomic_ssize;

#define git_atomic_ssize_set git_atomic64_set
#define git_atomic_ssize_add git_atomic64_add
#define git_atomic_ssize_get git_atomic64_get

#else

typedef git_atomic git_atomic_ssize;

#define git_atomic_ssize_set git_atomic_set
#define git_atomic_ssize_add git_atomic_add
#define git_atomic_ssize_get git_atomic_get

#endif

#ifdef GIT_THREADS

#ifdef GIT_WIN32
#   include "win32/thread.h"
#else
#   include "unix/pthread.h"
#endif

GIT_INLINE(void) git_atomic_set(git_atomic *a, int val)
{
#if defined(GIT_WIN32)
	InterlockedExchange(&a->val, (LONG)val);
#elif defined(GIT_BUILTIN_ATOMIC)
	__atomic_store_n(&a->val, val, __ATOMIC_SEQ_CST);
#elif defined(GIT_BUILTIN_SYNC)
	__sync_lock_test_and_set(&a->val, val);
#else
#	error "Unsupported architecture for atomic operations"
#endif
}

GIT_INLINE(int) git_atomic_inc(git_atomic *a)
{
#if defined(GIT_WIN32)
	return InterlockedIncrement(&a->val);
#elif defined(GIT_BUILTIN_ATOMIC)
	return __atomic_add_fetch(&a->val, 1, __ATOMIC_SEQ_CST);
#elif defined(GIT_BUILTIN_SYNC)
	return __sync_add_and_fetch(&a->val, 1);
#else
#	error "Unsupported architecture for atomic operations"
#endif
}

GIT_INLINE(int) git_atomic_add(git_atomic *a, int32_t addend)
{
#if defined(GIT_WIN32)
	return InterlockedExchangeAdd(&a->val, addend);
#elif defined(GIT_BUILTIN_ATOMIC)
	return __atomic_add_fetch(&a->val, addend, __ATOMIC_SEQ_CST);
#elif defined(GIT_BUILTIN_SYNC)
	return __sync_add_and_fetch(&a->val, addend);
#else
#	error "Unsupported architecture for atomic operations"
#endif
}

GIT_INLINE(int) git_atomic_dec(git_atomic *a)
{
#if defined(GIT_WIN32)
	return InterlockedDecrement(&a->val);
#elif defined(GIT_BUILTIN_ATOMIC)
	return __atomic_sub_fetch(&a->val, 1, __ATOMIC_SEQ_CST);
#elif defined(GIT_BUILTIN_SYNC)
	return __sync_sub_and_fetch(&a->val, 1);
#else
#	error "Unsupported architecture for atomic operations"
#endif
}

GIT_INLINE(int) git_atomic_get(git_atomic *a)
{
#if defined(GIT_WIN32)
	return (int)InterlockedCompareExchange(&a->val, 0, 0);
#elif defined(GIT_BUILTIN_ATOMIC)
	return __atomic_load_n(&a->val, __ATOMIC_SEQ_CST);
#elif defined(GIT_BUILTIN_SYNC)
	return __sync_val_compare_and_swap(&a->val, 0, 0);
#else
#	error "Unsupported architecture for atomic operations"
#endif
}

GIT_INLINE(void *) git___compare_and_swap(
	void * volatile *ptr, void *oldval, void *newval)
{
#if defined(GIT_WIN32)
	volatile void *foundval;
	foundval = InterlockedCompareExchangePointer((volatile PVOID *)ptr, newval, oldval);
	return (foundval == oldval) ? oldval : newval;
#elif defined(GIT_BUILTIN_ATOMIC)
	bool success = __atomic_compare_exchange(ptr, &oldval, &newval, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
	return success ? oldval : newval;
#elif defined(GIT_BUILTIN_SYNC)
	volatile void *foundval;
	foundval = __sync_val_compare_and_swap(ptr, oldval, newval);
	return (foundval == oldval) ? oldval : newval;
#else
#	error "Unsupported architecture for atomic operations"
#endif
}

GIT_INLINE(volatile void *) git___swap(
	void * volatile *ptr, void *newval)
{
#if defined(GIT_WIN32)
	return InterlockedExchangePointer(ptr, newval);
#elif defined(GIT_BUILTIN_ATOMIC)
	void * volatile foundval;
	__atomic_exchange(ptr, &newval, &foundval, __ATOMIC_SEQ_CST);
	return foundval;
#elif defined(GIT_BUILTIN_SYNC)
	return __sync_lock_test_and_set(ptr, newval);
#else
#	error "Unsupported architecture for atomic operations"
#endif
}

GIT_INLINE(volatile void *) git___load(void * volatile *ptr)
{
#if defined(GIT_WIN32)
	void *newval = NULL, *oldval = NULL;
	volatile void *foundval = NULL;
	foundval = InterlockedCompareExchangePointer((volatile PVOID *)ptr, newval, oldval);
	return foundval;
#elif defined(GIT_BUILTIN_ATOMIC)
	return (volatile void *)__atomic_load_n(ptr, __ATOMIC_SEQ_CST);
#elif defined(GIT_BUILTIN_SYNC)
	return (volatile void *)__sync_val_compare_and_swap(ptr, 0, 0);
#else
#	error "Unsupported architecture for atomic operations"
#endif
}

#ifdef GIT_ARCH_64

GIT_INLINE(int64_t) git_atomic64_add(git_atomic64 *a, int64_t addend)
{
#if defined(GIT_WIN32)
	return InterlockedExchangeAdd64(&a->val, addend);
#elif defined(GIT_BUILTIN_ATOMIC)
	return __atomic_add_fetch(&a->val, addend, __ATOMIC_SEQ_CST);
#elif defined(GIT_BUILTIN_SYNC)
	return __sync_add_and_fetch(&a->val, addend);
#else
#	error "Unsupported architecture for atomic operations"
#endif
}

GIT_INLINE(void) git_atomic64_set(git_atomic64 *a, int64_t val)
{
#if defined(GIT_WIN32)
	InterlockedExchange64(&a->val, val);
#elif defined(GIT_BUILTIN_ATOMIC)
	__atomic_store_n(&a->val, val, __ATOMIC_SEQ_CST);
#elif defined(GIT_BUILTIN_SYNC)
	__sync_lock_test_and_set(&a->val, val);
#else
#	error "Unsupported architecture for atomic operations"
#endif
}

GIT_INLINE(int64_t) git_atomic64_get(git_atomic64 *a)
{
#if defined(GIT_WIN32)
	return (int64_t)InterlockedCompareExchange64(&a->val, 0, 0);
#elif defined(GIT_BUILTIN_ATOMIC)
	return __atomic_load_n(&a->val, __ATOMIC_SEQ_CST);
#elif defined(GIT_BUILTIN_SYNC)
	return __sync_val_compare_and_swap(&a->val, 0, 0);
#else
#	error "Unsupported architecture for atomic operations"
#endif
}

#endif

#else

GIT_INLINE(int) git___noop(void) { return 0; }

#define git_thread unsigned int
#define git_thread_create(thread, start_routine, arg) git___noop()
#define git_thread_join(id, status) git___noop()

/* Pthreads Mutex */
#define git_mutex unsigned int
#define git_mutex_init(a) git___noop()
#define git_mutex_lock(a) git___noop()
#define git_mutex_unlock(a) git___noop()
#define git_mutex_free(a) git___noop()

/* Pthreads condition vars */
#define git_cond unsigned int
#define git_cond_init(c, a)	git___noop()
#define git_cond_free(c) git___noop()
#define git_cond_wait(c, l)	git___noop()
#define git_cond_signal(c) git___noop()
#define git_cond_broadcast(c) git___noop()

/* Pthreads rwlock */
#define git_rwlock unsigned int
#define git_rwlock_init(a)		git___noop()
#define git_rwlock_rdlock(a)	git___noop()
#define git_rwlock_rdunlock(a)	git___noop()
#define git_rwlock_wrlock(a)	git___noop()
#define git_rwlock_wrunlock(a)	git___noop()
#define git_rwlock_free(a)		git___noop()
#define GIT_RWLOCK_STATIC_INIT	0


GIT_INLINE(void) git_atomic_set(git_atomic *a, int val)
{
	a->val = val;
}

GIT_INLINE(int) git_atomic_inc(git_atomic *a)
{
	return ++a->val;
}

GIT_INLINE(int) git_atomic_add(git_atomic *a, int32_t addend)
{
	a->val += addend;
	return a->val;
}

GIT_INLINE(int) git_atomic_dec(git_atomic *a)
{
	return --a->val;
}

GIT_INLINE(int) git_atomic_get(git_atomic *a)
{
	return (int)a->val;
}

GIT_INLINE(void *) git___compare_and_swap(
	void * volatile *ptr, void *oldval, void *newval)
{
	if (*ptr == oldval)
		*ptr = newval;
	else
		oldval = newval;
	return oldval;
}

GIT_INLINE(volatile void *) git___swap(
	void * volatile *ptr, void *newval)
{
	volatile void *old = *ptr;
	*ptr = newval;
	return old;
}

GIT_INLINE(volatile void *) git___load(void * volatile *ptr)
{
	return *ptr;
}

#ifdef GIT_ARCH_64

GIT_INLINE(int64_t) git_atomic64_add(git_atomic64 *a, int64_t addend)
{
	a->val += addend;
	return a->val;
}

GIT_INLINE(void) git_atomic64_set(git_atomic64 *a, int64_t val)
{
	a->val = val;
}

GIT_INLINE(int64_t) git_atomic64_get(git_atomic64 *a)
{
	return (int64_t)a->val;
}

#endif

#endif

/* Atomically replace oldval with newval
 * @return oldval if it was replaced or newval if it was not
 */
#define git__compare_and_swap(P,O,N) \
	git___compare_and_swap((void * volatile *)P, O, N)

#define git__swap(ptr, val) (void *)git___swap((void * volatile *)&ptr, val)

#define git__load(ptr) (void *)git___load((void * volatile *)&ptr)

extern int git_online_cpus(void);

#if defined(GIT_THREADS)

# if defined(GIT_WIN32)
#  define GIT_MEMORY_BARRIER MemoryBarrier()
# elif defined(GIT_BUILTIN_ATOMIC)
#  define GIT_MEMORY_BARRIER __atomic_thread_fence(__ATOMIC_SEQ_CST)
# elif defined(GIT_BUILTIN_SYNC)
#  define GIT_MEMORY_BARRIER __sync_synchronize()
# endif

#else

# define GIT_MEMORY_BARRIER /* noop */

#endif

#endif
