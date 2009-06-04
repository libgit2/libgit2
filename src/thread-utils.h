#ifndef INCLUDE_thread_utils_h__
#define INCLUDE_thread_utils_h__

#if defined(GIT_HAS_PTHREAD)
typedef pthread_mutex_t git_lck;
# define GITLCK_INIT      PTHREAD_MUTEX_INITIALIZER
# define gitlck_init(a)   pthread_mutex_init(a, NULL)
# define gitlck_lock(a)   pthread_mutex_lock(a)
# define gitlck_unlock(a) pthread_mutex_unlock(a)
# define gitlck_free(a)   pthread_mutex_destroy(a)

# if defined(GIT_HAS_ASM_ATOMIC)
#  include <asm/atomic.h>
typedef atomic_t git_refcnt;
#  define gitrc_init(a)   atomic_set(a, 0)
#  define gitrc_inc(a)    atomic_inc_return(a)
#  define gitrc_dec(a)    atomic_dec_and_test(a)
#  define gitrc_free(a)   (void)0

# else
typedef struct { git_lck lock; int counter; } git_refcnt;

/** Initialize to 0.  No memory barrier is issued. */
GIT_INLINE(void) gitrc_init(git_refcnt *p)
{
	gitlck_init(&p->lock);
	p->counter = 0;
}

/**
 * Increment.
 *
 * Atomically increments @p by 1.  A memory barrier is also
 * issued before and after the operation.
 *
 * @param p pointer of type git_refcnt
 */
GIT_INLINE(void) gitrc_inc(git_refcnt *p)
{
	gitlck_lock(&p->lock);
	p->counter++;
	gitlck_unlock(&p->lock);
}

/**
 * Decrement and test.
 *
 * Atomically decrements @p by 1 and returns true if the
 * result is 0, or false for all other cases.  A memory
 * barrier is also issued before and after the operation.
 *
 * @param p pointer of type git_refcnt
 */
GIT_INLINE(int) gitrc_dec(git_refcnt *p)
{
	int c;
	gitlck_lock(&p->lock);
	c = --p->counter;
	gitlck_unlock(&p->lock);
	return !c;
}

/** Free any resources associated with the counter. */
#  define gitrc_free(p) gitlck_free(&(p)->lock)

# endif

#elif defined(GIT_THREADS)
# error GIT_THREADS but no git_lck implementation

#else
typedef struct { int dummy; } git_lck;
# define GIT_MUTEX_INIT   {}
# define gitlck_init(a)   (void)0
# define gitlck_lock(a)   (void)0
# define gitlck_unlock(a) (void)0
# define gitlck_free(a)   (void)0

typedef struct { int counter; } git_refcnt;
# define gitrc_init(a)   ((a)->counter = 0)
# define gitrc_inc(a)    ((a)->counter++)
# define gitrc_dec(a)    (--(a)->counter == 0)
# define gitrc_free(a)   (void)0

#endif

extern int git_online_cpus(void);

#endif /* INCLUDE_thread_utils_h__ */
