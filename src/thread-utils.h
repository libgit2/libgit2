#ifndef INCLUDE_thread_utils_h__
#define INCLUDE_thread_utils_h__



/* Common operations even if threading has been disabled */
typedef struct {
	volatile int val;
} git_atomic;

static inline void git_atomic_set(git_atomic *a, int val)
{
	a->val = val;
}

#ifdef GIT_THREADS

#define git_thread pthread_t
#define git_thread_create(thread, attr, start_routine, arg) pthread_create(thread, attr, start_routine, arg)
#define git_thread_kill(thread) pthread_cancel(thread)
#define git_thread_exit(status)	pthread_exit(status)
#define git_thread_join(id, status) pthread_join(id, status)

/* Pthreads Mutex */
#define git_mutex pthread_mutex_t
#define git_mutex_init(a)   pthread_mutex_init(a, NULL)
#define git_mutex_lock(a)   pthread_mutex_lock(a)
#define git_mutex_unlock(a) pthread_mutex_unlock(a)
#define git_mutex_free(a)   pthread_mutex_destroy(a)

/* Pthreads condition vars -- disabled by now */
#define git_cond unsigned int //pthread_cond_t
#define git_cond_init(c, a)	(void)0		//pthread_cond_init(c, a)
#define git_cond_free(c) (void)0		//pthread_cond_destroy(c)
#define git_cond_wait(c, l)	(void)0		//pthread_cond_wait(c, l)
#define git_cond_signal(c)	(void)0		//pthread_cond_signal(c)
#define git_cond_broadcast(c) (void)0	//pthread_cond_broadcast(c)

static inline int git_atomic_inc(git_atomic *a)
{
#ifdef __GNUC__
	return __sync_add_and_fetch(&a->val, 1);
#elif defined(_MSC_VER)
	return InterlockedIncrement(&a->val);
#else
#	error "Unsupported architecture for atomic operations"
#endif
}

static inline int git_atomic_dec(git_atomic *a)
{
#ifdef __GNUC__
	return __sync_sub_and_fetch(&a->val, 1);
#elif defined(_MSC_VER)
	return InterlockedDecrement(&a->val);
#else
#	error "Unsupported architecture for atomic operations"
#endif
}

#else

#define git_thread unsigned int
#define git_thread_create(thread, attr, start_routine, arg) (void)0
#define git_thread_kill(thread) (void)0
#define git_thread_exit(status) (void)0
#define git_thread_join(id, status) (void)0

/* Pthreads Mutex */
#define git_mutex unsigned int
#define git_mutex_init(a) (void)0
#define git_mutex_lock(a) (void)0
#define git_mutex_unlock(a) (void)0
#define git_mutex_free(a) (void)0

/* Pthreads condition vars */
#define git_cond unsigned int
#define git_cond_init(c, a)	(void)0
#define git_cond_free(c) (void)0
#define git_cond_wait(c, l)	(void)0
#define git_cond_signal(c) (void)0
#define git_cond_broadcast(c) (void)0

static inline int git_atomic_inc(git_atomic *a)
{
	return ++a->val;
}

static inline int git_atomic_dec(git_atomic *a)
{
	return --a->val;
}

#endif

extern int git_online_cpus(void);

#endif /* INCLUDE_thread_utils_h__ */
