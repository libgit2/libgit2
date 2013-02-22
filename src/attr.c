#include "repository.h"
#include "fileops.h"
#include "config.h"
#include "git2/oid.h"
#include <ctype.h>

GIT__USE_STRMAP;

const char *git_attr__true  = "[internal]__TRUE__";
const char *git_attr__false = "[internal]__FALSE__";
const char *git_attr__unset = "[internal]__UNSET__";

git_attr_t git_attr_value(const char *attr)
{
	if (attr == NULL || attr == git_attr__unset)
		return GIT_ATTR_UNSPECIFIED_T;

	if (attr == git_attr__true)
		return GIT_ATTR_TRUE_T;

	if (attr == git_attr__false)
		return GIT_ATTR_FALSE_T;

	return GIT_ATTR_VALUE_T;
}


static int collect_attr_files(
	git_repository *repo,
	uint32_t flags,
	const char *path,
	git_vector *files);


int git_attr_get(
	const char **value,
    git_repository *repo,
	uint32_t flags,
	const char *pathname,
	const char *name)
{
	int error;
	git_attr_path path;
	git_vector files = GIT_VECTOR_INIT;
	size_t i, j;
	git_attr_file *file;
	git_attr_name attr;
	git_attr_rule *rule;

	*value = NULL;

	if (git_attr_path__init(&path, pathname, git_repository_workdir(repo)) < 0)
		return -1;

	if ((error = collect_attr_files(repo, flags, pathname, &files)) < 0)
		goto cleanup;

	attr.name = name;
	attr.name_hash = git_attr_file__name_hash(name);

	git_vector_foreach(&files, i, file) {

		git_attr_file__foreach_matching_rule(file, &path, j, rule) {
			size_t pos;

			if (!git_vector_bsearch(&pos, &rule->assigns, &attr)) {
				*value = ((git_attr_assignment *)git_vector_get(
							  &rule->assigns, pos))->value;
				goto cleanup;
			}
		}
	}

cleanup:
	git_vector_free(&files);
	git_attr_path__free(&path);

	return error;
}


typedef struct {
	git_attr_name name;
	git_attr_assignment *found;
} attr_get_many_info;

int git_attr_get_many(
	const char **values,
    git_repository *repo,
	uint32_t flags,
	const char *pathname,
    size_t num_attr,
	const char **names)
{
	int error;
	git_attr_path path;
	git_vector files = GIT_VECTOR_INIT;
	size_t i, j, k;
	git_attr_file *file;
	git_attr_rule *rule;
	attr_get_many_info *info = NULL;
	size_t num_found = 0;

	memset((void *)values, 0, sizeof(const char *) * num_attr);

	if (git_attr_path__init(&path, pathname, git_repository_workdir(repo)) < 0)
		return -1;

	if ((error = collect_attr_files(repo, flags, pathname, &files)) < 0)
		goto cleanup;

	info = git__calloc(num_attr, sizeof(attr_get_many_info));
	GITERR_CHECK_ALLOC(info);

	git_vector_foreach(&files, i, file) {

		git_attr_file__foreach_matching_rule(file, &path, j, rule) {

			for (k = 0; k < num_attr; k++) {
				size_t pos;

				if (info[k].found != NULL) /* already found assignment */
					continue;

				if (!info[k].name.name) {
					info[k].name.name = names[k];
					info[k].name.name_hash = git_attr_file__name_hash(names[k]);
				}

				if (!git_vector_bsearch(&pos, &rule->assigns, &info[k].name)) {
					info[k].found = (git_attr_assignment *)
						git_vector_get(&rule->assigns, pos);
					values[k] = info[k].found->value;

					if (++num_found == num_attr)
						goto cleanup;
				}
			}
		}
	}

cleanup:
	git_vector_free(&files);
	git_attr_path__free(&path);
	git__free(info);

	return error;
}


int git_attr_foreach(
    git_repository *repo,
	uint32_t flags,
	const char *pathname,
	int (*callback)(const char *name, const char *value, void *payload),
	void *payload)
{
	int error;
	git_attr_path path;
	git_vector files = GIT_VECTOR_INIT;
	size_t i, j, k;
	git_attr_file *file;
	git_attr_rule *rule;
	git_attr_assignment *assign;
	git_strmap *seen = NULL;

	if (git_attr_path__init(&path, pathname, git_repository_workdir(repo)) < 0)
		return -1;

	if ((error = collect_attr_files(repo, flags, pathname, &files)) < 0)
		goto cleanup;

	seen = git_strmap_alloc();
	GITERR_CHECK_ALLOC(seen);

	git_vector_foreach(&files, i, file) {

		git_attr_file__foreach_matching_rule(file, &path, j, rule) {

			git_vector_foreach(&rule->assigns, k, assign) {
				/* skip if higher priority assignment was already seen */
				if (git_strmap_exists(seen, assign->name))
					continue;

				git_strmap_insert(seen, assign->name, assign, error);
				if (error < 0)
					goto cleanup;

				error = callback(assign->name, assign->value, payload);
				if (error) {
					giterr_clear();
					error = GIT_EUSER;
					goto cleanup;
				}
			}
		}
	}

cleanup:
	git_strmap_free(seen);
	git_vector_free(&files);
	git_attr_path__free(&path);

	return error;
}


int git_attr_add_macro(
	git_repository *repo,
	const char *name,
	const char *values)
{
	int error;
	git_attr_rule *macro = NULL;
	git_pool *pool;

	if (git_attr_cache__init(repo) < 0)
		return -1;

	macro = git__calloc(1, sizeof(git_attr_rule));
	GITERR_CHECK_ALLOC(macro);

	pool = &git_repository_attr_cache(repo)->pool;

	macro->match.pattern = git_pool_strdup(pool, name);
	GITERR_CHECK_ALLOC(macro->match.pattern);

	macro->match.length = strlen(macro->match.pattern);
	macro->match.flags = GIT_ATTR_FNMATCH_MACRO;

	error = git_attr_assignment__parse(repo, pool, &macro->assigns, &values);

	if (!error)
		error = git_attr_cache__insert_macro(repo, macro);

	if (error < 0)
		git_attr_rule__free(macro);

	return error;
}

bool git_attr_cache__is_cached(
	git_repository *repo, git_attr_file_source source, const char *path)
{
	git_buf cache_key = GIT_BUF_INIT;
	git_strmap *files = git_repository_attr_cache(repo)->files;
	const char *workdir = git_repository_workdir(repo);
	bool rval;

	if (workdir && git__prefixcmp(path, workdir) == 0)
		path += strlen(workdir);
	if (git_buf_printf(&cache_key, "%d#%s", (int)source, path) < 0)
		return false;

	rval = git_strmap_exists(files, git_buf_cstr(&cache_key));

	git_buf_free(&cache_key);

	return rval;
}

static int load_attr_file(
	const char **data,
	git_futils_filestamp *stamp,
	const char *filename)
{
	int error;
	git_buf content = GIT_BUF_INIT;

	error = git_futils_filestamp_check(stamp, filename);
	if (error < 0)
		return error;

	/* if error == 0, then file is up to date. By returning GIT_ENOTFOUND,
	 * we tell the caller not to reparse this file...
	 */
	if (!error)
		return GIT_ENOTFOUND;

	error = git_futils_readbuffer(&content, filename);
	if (error < 0) {
		/* convert error into ENOTFOUND so failed permissions / invalid
		 * file type don't actually stop the operation in progress.
		 */
		return GIT_ENOTFOUND;

		/* TODO: once warnings are available, issue a warning callback */
	}

	*data = git_buf_detach(&content);

	return 0;
}

static int load_attr_blob_from_index(
	const char **content,
	git_blob **blob,
	git_repository *repo,
	const git_oid *old_oid,
	const char *relfile)
{
	int error;
	size_t pos;
	git_index *index;
	const git_index_entry *entry;

	if ((error = git_repository_index__weakptr(&index, repo)) < 0 ||
		(error = git_index_find(&pos, index, relfile)) < 0)
		return error;

	entry = git_index_get_byindex(index, pos);

	if (old_oid && git_oid_cmp(old_oid, &entry->oid) == 0)
		return GIT_ENOTFOUND;

	if ((error = git_blob_lookup(blob, repo, &entry->oid)) < 0)
		return error;

	*content = git_blob_rawcontent(*blob);
	return 0;
}

static int load_attr_from_cache(
	git_attr_file **file,
	git_attr_cache *cache,
	git_attr_file_source source,
	const char *relative_path)
{
	git_buf  cache_key = GIT_BUF_INIT;
	khiter_t cache_pos;

	*file = NULL;

	if (!cache || !cache->files)
		return 0;

	if (git_buf_printf(&cache_key, "%d#%s", (int)source, relative_path) < 0)
		return -1;

	cache_pos = git_strmap_lookup_index(cache->files, cache_key.ptr);

	git_buf_free(&cache_key);

	if (git_strmap_valid_index(cache->files, cache_pos))
		*file = git_strmap_value_at(cache->files, cache_pos);

	return 0;
}

int git_attr_cache__internal_file(
	git_repository *repo,
	const char *filename,
	git_attr_file **file)
{
	int error = 0;
	git_attr_cache *cache = git_repository_attr_cache(repo);
	khiter_t cache_pos = git_strmap_lookup_index(cache->files, filename);

	if (git_strmap_valid_index(cache->files, cache_pos)) {
		*file = git_strmap_value_at(cache->files, cache_pos);
		return 0;
	}

	if (git_attr_file__new(file, 0, filename, &cache->pool) < 0)
		return -1;

	git_strmap_insert(cache->files, (*file)->key + 2, *file, error);
	if (error > 0)
		error = 0;

	return error;
}

int git_attr_cache__push_file(
	git_repository *repo,
	const char *base,
	const char *filename,
	git_attr_file_source source,
	git_attr_file_parser parse,
	void* parsedata,
	git_vector *stack)
{
	int error = 0;
	git_buf path = GIT_BUF_INIT;
	const char *workdir = git_repository_workdir(repo);
	const char *relfile, *content = NULL;
	git_attr_cache *cache = git_repository_attr_cache(repo);
	git_attr_file *file = NULL;
	git_blob *blob = NULL;
	git_futils_filestamp stamp;

	assert(filename && stack);

	/* join base and path as needed */
	if (base != NULL && git_path_root(filename) < 0) {
		if (git_buf_joinpath(&path, base, filename) < 0)
			return -1;
		filename = path.ptr;
	}

	relfile = filename;
	if (workdir && git__prefixcmp(relfile, workdir) == 0)
		relfile += strlen(workdir);

	/* check cache */
	if (load_attr_from_cache(&file, cache, source, relfile) < 0)
		return -1;

	/* if not in cache, load data, parse, and cache */

	if (source == GIT_ATTR_FILE_FROM_FILE) {
		git_futils_filestamp_set(
			&stamp, file ? &file->cache_data.stamp : NULL);

		error = load_attr_file(&content, &stamp, filename);
	} else {
		error = load_attr_blob_from_index(&content, &blob,
			repo, file ? &file->cache_data.oid : NULL, relfile);
	}

	if (error) {
		/* not finding a file is not an error for this function */
		if (error == GIT_ENOTFOUND) {
			giterr_clear();
			error = 0;
		}
		goto finish;
	}

	/* if we got here, we have to parse and/or reparse the file */
	if (file)
		git_attr_file__clear_rules(file);
	else {
		error = git_attr_file__new(&file, source, relfile, &cache->pool);
		if (error < 0)
			goto finish;
	}

	if (parse && (error = parse(repo, parsedata, content, file)) < 0)
		goto finish;

	git_strmap_insert(cache->files, file->key, file, error); //-V595
	if (error > 0)
		error = 0;

	/* remember "cache buster" file signature */
	if (blob)
		git_oid_cpy(&file->cache_data.oid, git_object_id((git_object *)blob));
	else
		git_futils_filestamp_set(&file->cache_data.stamp, &stamp);

finish:
	/* push file onto vector if we found one*/
	if (!error && file != NULL)
		error = git_vector_insert(stack, file);

	if (error != 0)
		git_attr_file__free(file);

	if (blob)
		git_blob_free(blob);
	else
		git__free((void *)content);

	git_buf_free(&path);

	return error;
}

#define push_attr_file(R,S,B,F) \
	git_attr_cache__push_file((R),(B),(F),GIT_ATTR_FILE_FROM_FILE,git_attr_file__parse_buffer,NULL,(S))

typedef struct {
	git_repository *repo;
	uint32_t flags;
	const char *workdir;
	git_index *index;
	git_vector *files;
} attr_walk_up_info;

int git_attr_cache__decide_sources(
	uint32_t flags, bool has_wd, bool has_index, git_attr_file_source *srcs)
{
	int count = 0;

	switch (flags & 0x03) {
	case GIT_ATTR_CHECK_FILE_THEN_INDEX:
		if (has_wd)
			srcs[count++] = GIT_ATTR_FILE_FROM_FILE;
		if (has_index)
			srcs[count++] = GIT_ATTR_FILE_FROM_INDEX;
		break;
	case GIT_ATTR_CHECK_INDEX_THEN_FILE:
		if (has_index)
			srcs[count++] = GIT_ATTR_FILE_FROM_INDEX;
		if (has_wd)
			srcs[count++] = GIT_ATTR_FILE_FROM_FILE;
		break;
	case GIT_ATTR_CHECK_INDEX_ONLY:
		if (has_index)
			srcs[count++] = GIT_ATTR_FILE_FROM_INDEX;
		break;
	}

	return count;
}

static int push_one_attr(void *ref, git_buf *path)
{
	int error = 0, n_src, i;
	attr_walk_up_info *info = (attr_walk_up_info *)ref;
	git_attr_file_source src[2];

	n_src = git_attr_cache__decide_sources(
		info->flags, info->workdir != NULL, info->index != NULL, src);

	for (i = 0; !error && i < n_src; ++i)
		error = git_attr_cache__push_file(
			info->repo, path->ptr, GIT_ATTR_FILE, src[i],
			git_attr_file__parse_buffer, NULL, info->files);

	return error;
}

static int collect_attr_files(
	git_repository *repo,
	uint32_t flags,
	const char *path,
	git_vector *files)
{
	int error;
	git_buf dir = GIT_BUF_INIT;
	const char *workdir = git_repository_workdir(repo);
	attr_walk_up_info info;

	if (git_attr_cache__init(repo) < 0 ||
		git_vector_init(files, 4, NULL) < 0)
		return -1;

	/* Resolve path in a non-bare repo */
	if (workdir != NULL)
		error = git_path_find_dir(&dir, path, workdir);
	else
		error = git_path_dirname_r(&dir, path);
	if (error < 0)
		goto cleanup;

	/* in precendence order highest to lowest:
	 * - $GIT_DIR/info/attributes
	 * - path components with .gitattributes
	 * - config core.attributesfile
	 * - $GIT_PREFIX/etc/gitattributes
	 */

	error = push_attr_file(
		repo, files, git_repository_path(repo), GIT_ATTR_FILE_INREPO);
	if (error < 0)
		goto cleanup;

	info.repo  = repo;
	info.flags = flags;
	info.workdir = workdir;
	if (git_repository_index__weakptr(&info.index, repo) < 0)
		giterr_clear(); /* no error even if there is no index */
	info.files = files;

	error = git_path_walk_up(&dir, workdir, push_one_attr, &info);
	if (error < 0)
		goto cleanup;

	if (git_repository_attr_cache(repo)->cfg_attr_file != NULL) {
		error = push_attr_file(
			repo, files, NULL, git_repository_attr_cache(repo)->cfg_attr_file);
		if (error < 0)
			goto cleanup;
	}

	if ((flags & GIT_ATTR_CHECK_NO_SYSTEM) == 0) {
		error = git_futils_find_system_file(&dir, GIT_ATTR_FILE_SYSTEM);
		if (!error)
			error = push_attr_file(repo, files, NULL, dir.ptr);
		else if (error == GIT_ENOTFOUND) {
			giterr_clear();
			error = 0;
		}
	}

 cleanup:
	if (error < 0)
		git_vector_free(files);
	git_buf_free(&dir);

	return error;
}

static char *try_global_default(const char *relpath)
{
	git_buf dflt = GIT_BUF_INIT;
	char *rval = NULL;

	if (!git_futils_find_global_file(&dflt, relpath))
		rval = git_buf_detach(&dflt);

	git_buf_free(&dflt);

	return rval;
}

int git_attr_cache__init(git_repository *repo)
{
	int ret;
	git_attr_cache *cache = git_repository_attr_cache(repo);
	git_config *cfg;

	if (cache->initialized)
		return 0;

	/* cache config settings for attributes and ignores */
	if (git_repository_config__weakptr(&cfg, repo) < 0)
		return -1;

	ret = git_config_get_string(&cache->cfg_attr_file, cfg, GIT_ATTR_CONFIG);
	if (ret < 0 && ret != GIT_ENOTFOUND)
		return ret;
	if (ret == GIT_ENOTFOUND)
		cache->cfg_attr_file = try_global_default(GIT_ATTR_CONFIG_DEFAULT);

	ret = git_config_get_string(&cache->cfg_excl_file, cfg, GIT_IGNORE_CONFIG);
	if (ret < 0 && ret != GIT_ENOTFOUND)
		return ret;
	if (ret == GIT_ENOTFOUND)
		cache->cfg_excl_file = try_global_default(GIT_IGNORE_CONFIG_DEFAULT);

	giterr_clear();

	/* allocate hashtable for attribute and ignore file contents */
	if (cache->files == NULL) {
		cache->files = git_strmap_alloc();
		GITERR_CHECK_ALLOC(cache->files);
	}

	/* allocate hashtable for attribute macros */
	if (cache->macros == NULL) {
		cache->macros = git_strmap_alloc();
		GITERR_CHECK_ALLOC(cache->macros);
	}

	/* allocate string pool */
	if (git_pool_init(&cache->pool, 1, 0) < 0)
		return -1;

	cache->initialized = 1;

	/* insert default macros */
	return git_attr_add_macro(repo, "binary", "-diff -crlf -text");
}

void git_attr_cache_flush(
	git_repository *repo)
{
	git_attr_cache *cache;

	if (!repo)
		return;

	cache = git_repository_attr_cache(repo);

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

	cache->initialized = 0;
}

int git_attr_cache__insert_macro(git_repository *repo, git_attr_rule *macro)
{
	git_strmap *macros = git_repository_attr_cache(repo)->macros;
	int error;

	/* TODO: generate warning log if (macro->assigns.length == 0) */
	if (macro->assigns.length == 0)
		return 0;

	git_strmap_insert(macros, macro->match.pattern, macro, error);
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

