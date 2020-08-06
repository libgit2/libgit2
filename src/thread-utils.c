/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "thread-utils.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#	define WIN32_LEAN_AND_MEAN
#endif
#	include <windows.h>
#elif defined(hpux) || defined(__hpux) || defined(_hpux)
#	include <sys/pstat.h>
#endif

/*
 * By doing this in two steps we can at least get
 * the function to be somewhat coherent, even
 * with this disgusting nest of #ifdefs.
 */
#ifndef _SC_NPROCESSORS_ONLN
#	ifdef _SC_NPROC_ONLN
#		define _SC_NPROCESSORS_ONLN _SC_NPROC_ONLN
#	elif defined _SC_CRAY_NCPU
#		define _SC_NPROCESSORS_ONLN _SC_CRAY_NCPU
#	endif
#endif

int git_online_cpus(void)
{
#ifdef _SC_NPROCESSORS_ONLN
	long ncpus;
#endif

#ifdef _WIN32
	SYSTEM_INFO info;
	GetSystemInfo(&info);

	if ((int)info.dwNumberOfProcessors > 0)
		return (int)info.dwNumberOfProcessors;
#elif defined(hpux) || defined(__hpux) || defined(_hpux)
	struct pst_dynamic psd;

	if (!pstat_getdynamic(&psd, sizeof(psd), (size_t)1, 0))
		return (int)psd.psd_proc_cnt;
#endif

#ifdef _SC_NPROCESSORS_ONLN
	if ((ncpus = (long)sysconf(_SC_NPROCESSORS_ONLN)) > 0)
		return (int)ncpus;
#endif

	return 1;
}

#ifndef GIT_THREADS

struct git_tls_data {
	void GIT_CALLBACK(free_fn)(void *payload);
	void *storage;
};

int git_tls_data__init(git_tls_data **out,
	void GIT_CALLBACK(free_fn)(void *storage))
{
	struct git_tls_data *tls = git__malloc(sizeof(struct git_tls_data));
	GIT_ERROR_CHECK_ALLOC(tls);

	tls->storage = NULL;
	tls->free_fn = free_fn;
	*out = tls;

	return 0;
}

int git_tls_data__set(git_tls_data *tls, void *payload)
{
	tls->storage = payload;
	return 0;
}

void *git_tls_data__get(git_tls_data *tls)
{
	return tls->storage;
}

void git_tls_data__free(git_tls_data *tls)
{
	tls->free_fn(tls->storage);
	git__free(tls);
}

#elif defined(GIT_WIN32)

struct git_tls_data {
	void GIT_CALLBACK(free_fn)(void *payload);
	DWORD fls_index;
};

struct git_tls_cell {
	void GIT_CALLBACK(free_fn)(void *storage);
	void *storage;
};

static void WINAPI git_tls_cell__free(void *sc)
{
  struct git_tls_cell *storage_cell = sc;
	if (storage_cell == NULL) {
		return;
	}

	storage_cell->free_fn(storage_cell->storage);
	git__free(storage_cell);
}

int git_tls_data__init(git_tls_data **out,
	void GIT_CALLBACK(free_fn)(void *payload))
{
	struct git_tls_data *tls = git__malloc(sizeof(struct git_tls_data));
	GIT_ERROR_CHECK_ALLOC(tls);

	if ((tls->fls_index = FlsAlloc(git_tls_cell__free)) == FLS_OUT_OF_INDEXES) {
		git__free(tls);
		return -1;
	}

	tls->free_fn = free_fn;
	*out = tls;

	return 0;
}

int git_tls_data__set(git_tls_data *tls, void *payload)
{
	struct git_tls_cell *storage_cell;

	if (payload == NULL) {
		if ((storage_cell = FlsGetValue(tls->fls_index)) != NULL)
			git_tls_cell__free(storage_cell);

		if (FlsSetValue(tls->fls_index, NULL) == 0)
			return -1;

		return 0;
	}

	storage_cell = git__malloc(sizeof(struct git_tls_cell));
	GIT_ERROR_CHECK_ALLOC(storage_cell);

	storage_cell->free_fn = tls->free_fn;
	storage_cell->storage = payload;

	if (FlsSetValue(tls->fls_index, storage_cell) == 0) {
		git__free(storage_cell);
		return -1;
	}

	return 0;
}

void *git_tls_data__get(git_tls_data *tls)
{
	struct git_tls_cell *storage_cell = FlsGetValue(tls->fls_index);
	if (storage_cell == NULL)
		return NULL;

	return storage_cell->storage;
}

void git_tls_data__free(git_tls_data *tls)
{
	FlsFree(tls->fls_index);
	tls->free_fn = NULL;
	git__free(tls);
}

#elif defined(_POSIX_THREADS)

struct git_tls_data {
	void GIT_CALLBACK(free_fn)(void *payload);
	pthread_key_t tls_key;
};

struct git_tls_cell {
	void GIT_CALLBACK(free_fn)(void *storage);
	void *storage;
};

static void git_tls_cell__free(void *sc)
{
  struct git_tls_cell *storage_cell = sc;
	storage_cell->free_fn(storage_cell->storage);
	git__free(storage_cell);
}

int git_tls_data__init(git_tls_data **out,
	void GIT_CALLBACK(free_fn)(void *payload))
{
	struct git_tls_data *tls = git__malloc(sizeof(struct git_tls_data));
	GIT_ERROR_CHECK_ALLOC(tls);

	if (pthread_key_create(&tls->tls_key, git_tls_cell__free) != 0) {
		git__free(tls);
		return -1;
	}

	tls->free_fn = free_fn;
	*out = tls;

	return 0;
}

int git_tls_data__set(git_tls_data *tls, void *payload)
{
	struct git_tls_cell *storage_cell;

	if (payload == NULL) {
		if ((storage_cell = pthread_getspecific(tls->tls_key)) != NULL)
			git_tls_cell__free(storage_cell);

		if (pthread_setspecific(tls->tls_key, NULL) != 0)
			return -1;

		return 0;
	}

	storage_cell = git__malloc(sizeof(struct git_tls_cell));
	GIT_ERROR_CHECK_ALLOC(storage_cell);

	storage_cell->free_fn = tls->free_fn;
	storage_cell->storage = payload;

	if (pthread_setspecific(tls->tls_key, storage_cell) != 0) {
		git__free(storage_cell);
		return -1;
	}

	return 0;
}

void *git_tls_data__get(git_tls_data *tls)
{
	struct git_tls_cell *storage_cell = pthread_getspecific(tls->tls_key);
	if (storage_cell == NULL)
		return NULL;

	return storage_cell->storage;
}

void git_tls_data__free(git_tls_data *tls)
{
	git_tls_data__set(tls, NULL);
	pthread_key_delete(tls->tls_key);
	git__free(tls);
}

#else
#  error unknown threading model
#endif
