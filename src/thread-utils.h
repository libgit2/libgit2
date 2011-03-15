#ifndef INCLUDE_thread_utils_h__
#define INCLUDE_thread_utils_h__


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

/* Pthreads condition vars */
#define git_cond pthread_cond_t
#define git_cond_init(c, a)	pthread_cond_init(c, a)
#define git_cond_free(c)		pthread_cond_destroy(c)
#define git_cond_wait(c, l)	pthread_cond_wait(c, l)
#define git_cond_signal(c)		pthread_cond_signal(c)
#define git_cond_broadcast(c)	pthread_cond_broadcast(c)

extern int git_online_cpus(void);

#endif /* INCLUDE_thread_utils_h__ */
