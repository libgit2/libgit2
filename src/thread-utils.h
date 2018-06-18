/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_thread_utils_h__
#define INCLUDE_thread_utils_h__

#if defined(GIT_THREADS)
# if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
#  define __GIT_THREADS_USE_C11 1
#  include <stdatomic.h>
# elif defined(__GNUC__) && __GNUC__ >= 4
#  define __GIT_THREADS_USE_GNU 1
# elif defined(GIT_WIN32)
#  define __GIT_THREADS_USE_WIN32 1
# else
#  error Atomic primitives do not exist on this compiler; configure libgit2 with -DGIT_THREADS=0
# endif
#endif

typedef struct {
#if __GIT_THREADS_USE_C11
    atomic_int val;
#elif __GIT_THREADS_USE_WIN32
	volatile long val;
#else // __GIT_THREADS_USE_GNU || true; fallback case
	volatile int val;
#endif
} git_atomic;

#ifdef GIT_ARCH_64

typedef struct {
#if __GIT_THREADS_USE_C11
    atomic_int_fast64_t val;
#elif __GIT_THREADS_USE_WIN32
	volatile __int64 val;
#else // __GIT_THREADS_USE_GNU || true; fallback case
	volatile int64_t val;
#endif
} git_atomic64;

typedef git_atomic64 git_atomic_ssize;

#define git_atomic_ssize_add git_atomic64_add

#else

typedef git_atomic git_atomic_ssize;

#define git_atomic_ssize_add git_atomic_add

#endif

GIT_INLINE(void) git_atomic_set(git_atomic *a, int val)
{
#if __GIT_THREADS_USE_C11
    atomic_store(&a->val, val);
#elif __GIT_THREADS_USE_WIN32
	InterlockedExchange(&a->val, (LONG)val);
#elif __GIT_THREADS_USE_GNU
	__sync_lock_test_and_set(&a->val, val);
#else
    a->val = val;
#endif
}

GIT_INLINE(int) git_atomic_get(git_atomic *a)
{
#if __GIT_THREADS_USE_C11
    return atomic_load(&a->val);
#else
    return (int)a->val;
#endif
}

GIT_INLINE(int) git_atomic_inc(git_atomic *a)
{
#if __GIT_THREADS_USE_C11
    return atomic_fetch_add(&a->val, 1) + 1;
#elif __GIT_THREADS_USE_WIN32
    return InterlockedIncrement(&a->val);
#elif __GIT_THREADS_USE_GNU
    return __sync_add_and_fetch(&a->val, 1);
#else
    return ++a->val;
#endif
}

GIT_INLINE(int) git_atomic_add(git_atomic *a, int32_t addend)
{
#if __GIT_THREADS_USE_C11
    return atomic_fetch_add(&a->val, addend) + addend;
#elif __GIT_THREADS_USE_WIN32
    return InterlockedExchangeAdd(&a->val, addend);
#elif __GIT_THREADS_USE_GNU
    return __sync_add_and_fetch(&a->val, addend);
#else
    a->val += addend;
    return a->val;
#endif
}

GIT_INLINE(int) git_atomic_dec(git_atomic *a)
{
#if __GIT_THREADS_USE_C11
    return atomic_fetch_sub(&a->val, 1) - 1;
#elif __GIT_THREADS_USE_WIN32
    return InterlockedDecrement(&a->val);
#elif __GIT_THREADS_USE_GNU
    return __sync_sub_and_fetch(&a->val, 1);
#else
    return --a->val;
#endif
}

GIT_INLINE(void *) git___compare_and_swap(
	void * volatile *ptr, void *oldval, void *newval)
{
#if __GIT_THREADS_USE_C11
    return atomic_compare_exchange_strong((volatile _Atomic(void *) *)ptr, &oldval, newval) ? oldval : newval;
#elif __GIT_THREADS_USE_WIN32
    volatile void *foundval;
	foundval = InterlockedCompareExchangePointer((volatile PVOID *)ptr, newval, oldval);
    return (foundval == oldval) ? oldval : newval;
#elif __GIT_THREADS_USE_GNU
    volatile void *foundval;
	foundval = __sync_val_compare_and_swap(ptr, oldval, newval);
    return (foundval == oldval) ? oldval : newval;
#else
	if (*ptr == oldval)
		*ptr = newval;
	else
		oldval = newval;
	return oldval;
#endif
}

GIT_INLINE(volatile void *) git___swap(
	void * volatile *ptr, void *newval)
{
#if __GIT_THREADS_USE_C11
    return atomic_exchange((volatile _Atomic(void *) *)ptr, newval);
#elif __GIT_THREADS_USE_WIN32
	return InterlockedExchangePointer(ptr, newval);
#elif __GIT_THREADS_USE_GNU
	return __sync_lock_test_and_set(ptr, newval);
#else
    volatile void *old = *ptr;
    *ptr = newval;
    return old;
#endif
}

#ifdef GIT_ARCH_64

GIT_INLINE(int64_t) git_atomic64_add(git_atomic64 *a, int64_t addend)
{
# if __GIT_THREADS_USE_C11
    return atomic_fetch_add(&a->val, addend) + addend;
# elif __GIT_THREADS_USE_WIN32
    return InterlockedExchangeAdd64(&a->val, addend);
# elif __GIT_THREADS_USE_GNU
    return __sync_add_and_fetch(&a->val, addend);
# else
    a->val += addend;
    return a->val;
# endif
}

#endif

/* Atomically replace oldval with newval
 * @return oldval if it was replaced or newval if it was not
 */
#define git__compare_and_swap(P,O,N) \
	git___compare_and_swap((void * volatile *)P, O, N)

#define git__swap(ptr, val) (void *)git___swap((void * volatile *)&ptr, val)

extern int git_online_cpus(void);

#if __GIT_THREADS_USE_C11
#  define GIT_MEMORY_BARRIER atomic_thread_fence(memory_order_seq_cst)
#elif __GIT_THREADS_USE_WIN32
# define GIT_MEMORY_BARRIER MemoryBarrier()
#elif __GIT_THREADS_USE_GNU
# define GIT_MEMORY_BARRIER __sync_synchronize()
#else
# define GIT_MEMORY_BARRIER /* noop */
#endif

#ifdef GIT_THREADS

# ifdef GIT_WIN32
#  include "win32/thread.h"
# else
#  include "unix/pthread.h"
#endif

#else

# define git_thread unsigned int
# define git_thread_create(thread, start_routine, arg) 0
# define git_thread_join(id, status) (void)0

/* Pthreads Mutex */
# define git_mutex unsigned int
GIT_INLINE(int) git_mutex_init(git_mutex *mutex) \
{ GIT_UNUSED(mutex); return 0; }
GIT_INLINE(int) git_mutex_lock(git_mutex *mutex) \
{ GIT_UNUSED(mutex); return 0; }
# define git_mutex_unlock(a) (void)0
# define git_mutex_free(a) (void)0

/* Pthreads condition vars */
# define git_cond unsigned int
# define git_cond_init(c, a)    (void)0
# define git_cond_free(c) (void)0
# define git_cond_wait(c, l)    (void)0
# define git_cond_signal(c) (void)0
# define git_cond_broadcast(c) (void)0

/* Pthreads rwlock */
# define git_rwlock unsigned int
# define git_rwlock_init(a)        0
# define git_rwlock_rdlock(a)    0
# define git_rwlock_rdunlock(a)    (void)0
# define git_rwlock_wrlock(a)    0
# define git_rwlock_wrunlock(a)    (void)0
# define git_rwlock_free(a)        (void)0
# define GIT_RWLOCK_STATIC_INIT    0

#endif

#endif /* INCLUDE_thread_utils_h__ */
