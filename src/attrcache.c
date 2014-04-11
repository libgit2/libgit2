#include "common.h"
#include "repository.h"
#include "attr_file.h"
#include "config.h"
#include "sysdir.h"
#include "ignore.h"

GIT__USE_STRMAP;

GIT_INLINE(int) attr_cache_lock(git_attr_cache *cache)
{
	GIT_UNUSED(cache); /* avoid warning if threading is off */

	if (git_mutex_lock(&cache->lock) < 0) {
		giterr_set(GITERR_OS, "Unable to get attr cache lock");
		return -1;
	}
	return 0;
}

GIT_INLINE(void) attr_cache_unlock(git_attr_cache *cache)
{
	GIT_UNUSED(cache); /* avoid warning if threading is off */
	git_mutex_unlock(&cache->lock);
}

GIT_INLINE(git_attr_cache_entry *) attr_cache_lookup_entry(
	git_attr_cache *cache, const char *path)
{
	khiter_t pos = git_strmap_lookup_index(cache->files, path);

	if (git_strmap_valid_index(cache->files, pos))
		return git_strmap_value_at(cache->files, pos);
	else
		return NULL;
}

int git_attr_cache_entry__new(
	git_attr_cache_entry **out,
	const char *base,
	const char *path,
	git_pool *pool)
{
	size_t baselen = base ? strlen(base) : 0, pathlen = strlen(path);
	size_t cachesize = sizeof(git_attr_cache_entry) + baselen + pathlen + 1;
	git_attr_cache_entry *ce;

	ce = git_pool_mallocz(pool, cachesize);
	GITERR_CHECK_ALLOC(ce);

	if (baselen)
		memcpy(ce->fullpath, base, baselen);
	memcpy(&ce->fullpath[baselen], path, pathlen);
	ce->path = &ce->fullpath[baselen];
	*out = ce;

	return 0;
}

/* call with attrcache locked */
static int attr_cache_make_entry(
	git_attr_cache_entry **out, git_repository *repo, const char *path)
{
	int error = 0;
	git_attr_cache *cache = git_repository_attr_cache(repo);
	git_attr_cache_entry *ce = NULL;

	error = git_attr_cache_entry__new(
		&ce, git_repository_workdir(repo), path, &cache->pool);

	if (!error) {
		git_strmap_insert(cache->files, ce->path, ce, error);
		if (error > 0)
			error = 0;
	}

	*out = ce;
	return error;
}

/* insert entry or replace existing if we raced with another thread */
static int attr_cache_upsert(git_attr_cache *cache, git_attr_file *file)
{
	git_attr_cache_entry *ce;
	git_attr_file *old;

	if (attr_cache_lock(cache) < 0)
		return -1;

	ce = attr_cache_lookup_entry(cache, file->ce->path);

	old = ce->file[file->source];

	GIT_REFCOUNT_OWN(file, ce);
	GIT_REFCOUNT_INC(file);
	ce->file[file->source] = file;

	if (old) {
		GIT_REFCOUNT_OWN(old, NULL);
		git_attr_file__free(old);
	}

	attr_cache_unlock(cache);
	return 0;
}

static int attr_cache_remove(git_attr_cache *cache, git_attr_file *file)
{
	int error = 0;
	git_attr_cache_entry *ce;
	bool found = false;

	if (!file)
		return 0;
	if ((error = attr_cache_lock(cache)) < 0)
		return error;

	if ((ce = attr_cache_lookup_entry(cache, file->ce->path)) != NULL &&
		ce->file[file->source] == file)
	{
		ce->file[file->source] = NULL;
		found = true;
	}

	attr_cache_unlock(cache);

	if (found)
		git_attr_file__free(file);

	return error;
}

int git_attr_cache__get(
	git_attr_file **out,
	git_repository *repo,
	git_attr_cache_source source,
	const char *base,
	const char *filename,
	git_attr_cache_parser parser,
	void *payload)
{
	int error = 0;
	git_buf path = GIT_BUF_INIT;
	const char *wd = git_repository_workdir(repo), *relfile;
	git_attr_cache *cache = git_repository_attr_cache(repo);
	git_attr_cache_entry *ce = NULL;
	git_attr_file *file = NULL;

	/* join base and path as needed */
	if (base != NULL && git_path_root(filename) < 0) {
		if (git_buf_joinpath(&path, base, filename) < 0)
			return -1;
		filename = path.ptr;
	}

	relfile = filename;
	if (wd && !git__prefixcmp(relfile, wd))
		relfile += strlen(wd);

	/* check cache for existing entry */
	if ((error = attr_cache_lock(cache)) < 0)
		goto cleanup;

	ce = attr_cache_lookup_entry(cache, relfile);
	if (!ce) {
		if ((error = attr_cache_make_entry(&ce, repo, relfile)) < 0)
			goto cleanup;
	} else if (ce->file[source] != NULL) {
		file = ce->file[source];
		GIT_REFCOUNT_INC(file);
	}

	attr_cache_unlock(cache);

	/* if this is not a file backed entry, just create a new empty one */
	if (!parser) {
		error = git_attr_file__new(&file, ce, source);
		goto cleanup;
	}

	/* otherwise load and/or reload as needed */
	switch (git_attr_file__out_of_date(repo, file)) {
	case 1:
		if (!(error = git_attr_file__load(
				  &file, repo, ce, source, parser, payload)))
			error = attr_cache_upsert(cache, file);
		break;
	case 0:
		/* just use the file */
		break;
	case GIT_ENOTFOUND:
		/* did exist and now does not - remove from cache */
		error = attr_cache_remove(cache, file);
		file = NULL;
		break;
	default:
		/* other error (e.g. out of memory, can't read index) */
		giterr_clear();
		break;
	}

cleanup:
	*out = error ? NULL : file;
	git_buf_free(&path);
	return error;
}

bool git_attr_cache__is_cached(
	git_repository *repo,
	git_attr_cache_source source,
	const char *filename)
{
	git_attr_cache *cache = git_repository_attr_cache(repo);
	git_strmap *files;
	khiter_t pos;
	git_attr_cache_entry *ce;

	if (!(cache = git_repository_attr_cache(repo)) ||
		!(files = cache->files))
		return false;

	pos = git_strmap_lookup_index(files, filename);
	if (!git_strmap_valid_index(files, pos))
		return false;

	ce = git_strmap_value_at(files, pos);

	return ce && (ce->file[source] != NULL);
}


static int attr_cache__lookup_path(
	char **out, git_config *cfg, const char *key, const char *fallback)
{
	git_buf buf = GIT_BUF_INIT;
	int error;
	const git_config_entry *entry = NULL;

	*out = NULL;

	if ((error = git_config__lookup_entry(&entry, cfg, key, false)) < 0)
		return error;

	if (entry) {
		const char *cfgval = entry->value;

		/* expand leading ~/ as needed */
		if (cfgval && cfgval[0] == '~' && cfgval[1] == '/' &&
			!git_sysdir_find_global_file(&buf, &cfgval[2]))
			*out = git_buf_detach(&buf);
		else if (cfgval)
			*out = git__strdup(cfgval);

	}
	else if (!git_sysdir_find_xdg_file(&buf, fallback))
		*out = git_buf_detach(&buf);

	git_buf_free(&buf);

	return error;
}

static void attr_cache__free(git_attr_cache *cache)
{
	if (!cache)
		return;

	if (cache->files != NULL) {
		git_attr_file *file;

		git_strmap_foreach_value(cache->files, file, {
			git_attr_file__free(file);
		});
		git_strmap_free(cache->files);
	}

	if (cache->macros != NULL) {
		git_attr_rule *rule;

		git_strmap_foreach_value(cache->macros, rule, {
			git_attr_rule__free(rule);
		});
		git_strmap_free(cache->macros);
	}

	git_pool_clear(&cache->pool);

	git__free(cache->cfg_attr_file);
	cache->cfg_attr_file = NULL;

	git__free(cache->cfg_excl_file);
	cache->cfg_excl_file = NULL;

	git_mutex_free(&cache->lock);

	git__free(cache);
}

int git_attr_cache__init(git_repository *repo)
{
	int ret = 0;
	git_attr_cache *cache = git_repository_attr_cache(repo);
	git_config *cfg;

	if (cache)
		return 0;

	if ((ret = git_repository_config__weakptr(&cfg, repo)) < 0)
		return ret;

	cache = git__calloc(1, sizeof(git_attr_cache));
	GITERR_CHECK_ALLOC(cache);

	/* set up lock */
	if (git_mutex_init(&cache->lock) < 0) {
		giterr_set(GITERR_OS, "Unable to initialize lock for attr cache");
		git__free(cache);
		return -1;
	}

	/* cache config settings for attributes and ignores */
	ret = attr_cache__lookup_path(
		&cache->cfg_attr_file, cfg, GIT_ATTR_CONFIG, GIT_ATTR_FILE_XDG);
	if (ret < 0)
		goto cancel;

	ret = attr_cache__lookup_path(
		&cache->cfg_excl_file, cfg, GIT_IGNORE_CONFIG, GIT_IGNORE_FILE_XDG);
	if (ret < 0)
		goto cancel;

	/* allocate hashtable for attribute and ignore file contents,
	 * hashtable for attribute macros, and string pool
	 */
	if ((ret = git_strmap_alloc(&cache->files)) < 0 ||
		(ret = git_strmap_alloc(&cache->macros)) < 0 ||
		(ret = git_pool_init(&cache->pool, 1, 0)) < 0)
		goto cancel;

	cache = git__compare_and_swap(&repo->attrcache, NULL, cache);
	if (cache)
		goto cancel; /* raced with another thread, free this but no error */

	/* insert default macros */
	return git_attr_add_macro(repo, "binary", "-diff -crlf -text");

cancel:
	attr_cache__free(cache);
	return ret;
}

void git_attr_cache_flush(git_repository *repo)
{
	git_attr_cache *cache;

	/* this could be done less expensively, but for now, we'll just free
	 * the entire attrcache and let the next use reinitialize it...
	 */
	if (repo && (cache = git__swap(repo->attrcache, NULL)) != NULL)
		attr_cache__free(cache);
}

int git_attr_cache__insert_macro(git_repository *repo, git_attr_rule *macro)
{
	git_attr_cache *cache = git_repository_attr_cache(repo);
	git_strmap *macros = cache->macros;
	int error;

	/* TODO: generate warning log if (macro->assigns.length == 0) */
	if (macro->assigns.length == 0)
		return 0;

	if (git_mutex_lock(&cache->lock) < 0) {
		giterr_set(GITERR_OS, "Unable to get attr cache lock");
		error = -1;
	} else {
		git_strmap_insert(macros, macro->match.pattern, macro, error);
		git_mutex_unlock(&cache->lock);
	}

	return (error < 0) ? -1 : 0;
}

git_attr_rule *git_attr_cache__lookup_macro(
	git_repository *repo, const char *name)
{
	git_strmap *macros = git_repository_attr_cache(repo)->macros;
	khiter_t pos;

	pos = git_strmap_lookup_index(macros, name);

	if (!git_strmap_valid_index(macros, pos))
		return NULL;

	return (git_attr_rule *)git_strmap_value_at(macros, pos);
}

