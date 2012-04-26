#include "repository.h"
#include "fileops.h"
#include "config.h"
#include <ctype.h>

static int collect_attr_files(
	git_repository *repo, const char *path, git_vector *files);


int git_attr_get(
    git_repository *repo, const char *pathname,
	const char *name, const char **value)
{
	int error;
	git_attr_path path;
	git_vector files = GIT_VECTOR_INIT;
	unsigned int i, j;
	git_attr_file *file;
	git_attr_name attr;
	git_attr_rule *rule;

	*value = NULL;

	if ((error = git_attr_path__init(
			&path, pathname, git_repository_workdir(repo))) < 0 ||
		(error = collect_attr_files(repo, pathname, &files)) < 0)
		return error;

	attr.name = name;
	attr.name_hash = git_attr_file__name_hash(name);

	git_vector_foreach(&files, i, file) {

		git_attr_file__foreach_matching_rule(file, &path, j, rule) {
			int pos = git_vector_bsearch(&rule->assigns, &attr);
			if (pos >= 0) {
				*value = ((git_attr_assignment *)git_vector_get(
							  &rule->assigns, pos))->value;
				goto found;
			}
		}
	}

found:
	git_vector_free(&files);

	return error;
}


typedef struct {
	git_attr_name name;
	git_attr_assignment *found;
} attr_get_many_info;

int git_attr_get_many(
    git_repository *repo, const char *pathname,
    size_t num_attr, const char **names, const char **values)
{
	int error;
	git_attr_path path;
	git_vector files = GIT_VECTOR_INIT;
	unsigned int i, j, k;
	git_attr_file *file;
	git_attr_rule *rule;
	attr_get_many_info *info = NULL;
	size_t num_found = 0;

	memset((void *)values, 0, sizeof(const char *) * num_attr);

	if ((error = git_attr_path__init(
			&path, pathname, git_repository_workdir(repo))) < 0 ||
		(error = collect_attr_files(repo, pathname, &files)) < 0)
		return error;

	info = git__calloc(num_attr, sizeof(attr_get_many_info));
	GITERR_CHECK_ALLOC(info);

	git_vector_foreach(&files, i, file) {

		git_attr_file__foreach_matching_rule(file, &path, j, rule) {

			for (k = 0; k < num_attr; k++) {
				int pos;

				if (info[k].found != NULL) /* already found assignment */
					continue;

				if (!info[k].name.name) {
					info[k].name.name = names[k];
					info[k].name.name_hash = git_attr_file__name_hash(names[k]);
				}

				pos = git_vector_bsearch(&rule->assigns, &info[k].name);
				if (pos >= 0) {
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
	git__free(info);

	return error;
}


int git_attr_foreach(
    git_repository *repo, const char *pathname,
	int (*callback)(const char *name, const char *value, void *payload),
	void *payload)
{
	int error;
	git_attr_path path;
	git_vector files = GIT_VECTOR_INIT;
	unsigned int i, j, k;
	git_attr_file *file;
	git_attr_rule *rule;
	git_attr_assignment *assign;
	git_hashtable *seen = NULL;

	if ((error = git_attr_path__init(
			&path, pathname, git_repository_workdir(repo))) < 0 ||
		(error = collect_attr_files(repo, pathname, &files)) < 0)
		return error;

	seen = git_hashtable_alloc(8, git_hash__strhash_cb, git_hash__strcmp_cb);
	GITERR_CHECK_ALLOC(seen);

	git_vector_foreach(&files, i, file) {

		git_attr_file__foreach_matching_rule(file, &path, j, rule) {

			git_vector_foreach(&rule->assigns, k, assign) {
				/* skip if higher priority assignment was already seen */
				if (git_hashtable_lookup(seen, assign->name))
					continue;

				if (!(error = git_hashtable_insert(seen, assign->name, assign)))
					error = callback(assign->name, assign->value, payload);

				if (error != 0)
					goto cleanup;
			}
		}
	}

cleanup:
	git_hashtable_free(seen);
	git_vector_free(&files);

	return error;
}


int git_attr_add_macro(
	git_repository *repo,
	const char *name,
	const char *values)
{
	int error;
	git_attr_rule *macro = NULL;

	if (git_attr_cache__init(repo) < 0)
		return -1;

	macro = git__calloc(1, sizeof(git_attr_rule));
	GITERR_CHECK_ALLOC(macro);

	macro->match.pattern = git__strdup(name);
	GITERR_CHECK_ALLOC(macro->match.pattern);

	macro->match.length = strlen(macro->match.pattern);
	macro->match.flags = GIT_ATTR_FNMATCH_MACRO;

	error = git_attr_assignment__parse(repo, &macro->assigns, &values);

	if (!error)
		error = git_attr_cache__insert_macro(repo, macro);

	if (error < 0)
		git_attr_rule__free(macro);

	return error;
}

bool git_attr_cache__is_cached(git_repository *repo, const char *path)
{
	const char *cache_key = path;
	if (repo && git__prefixcmp(cache_key, git_repository_workdir(repo)) == 0)
		cache_key += strlen(git_repository_workdir(repo));
	return (git_hashtable_lookup(
		git_repository_attr_cache(repo)->files, cache_key) != NULL);
}

int git_attr_cache__lookup_or_create_file(
	git_repository *repo,
	const char *key,
	const char *filename,
	int (*loader)(git_repository *, const char *, git_attr_file *),
	git_attr_file **file_ptr)
{
	int error;
	git_attr_cache *cache = git_repository_attr_cache(repo);
	git_attr_file *file = NULL;

	if ((file = git_hashtable_lookup(cache->files, key)) != NULL) {
		*file_ptr = file;
		return 0;
	}

	if (loader && git_path_exists(filename) == false) {
		*file_ptr = NULL;
		return 0;
	}

	if (git_attr_file__new(&file) < 0)
		return -1;

	if (loader)
		error = loader(repo, filename, file);
	else
		error = git_attr_file__set_path(repo, key, file);

	if (!error)
		error = git_hashtable_insert(cache->files, file->path, file);

	if (error < 0) {
		git_attr_file__free(file);
		file = NULL;
	}

	*file_ptr = file;
	return error;
}

/* add git_attr_file to vector of files, loading if needed */
int git_attr_cache__push_file(
	git_repository *repo,
	git_vector     *stack,
	const char     *base,
	const char     *filename,
	int (*loader)(git_repository *, const char *, git_attr_file *))
{
	int error;
	git_buf path = GIT_BUF_INIT;
	git_attr_file *file = NULL;
	const char *cache_key, *workdir;

	if (base != NULL) {
		if (git_buf_joinpath(&path, base, filename) < 0)
			return -1;
		filename = path.ptr;
	}

	/* either get attr_file from cache or read from disk */
	cache_key = filename;
	workdir = git_repository_workdir(repo);

	if (repo && workdir && git__prefixcmp(cache_key, workdir) == 0)
		cache_key += strlen(workdir);

	error = git_attr_cache__lookup_or_create_file(
		repo, cache_key, filename, loader, &file);

	if (!error && file != NULL)
		error = git_vector_insert(stack, file);

	git_buf_free(&path);
	return error;
}

#define push_attrs(R,S,B,F) \
	git_attr_cache__push_file((R),(S),(B),(F),git_attr_file__from_file)

typedef struct {
	git_repository *repo;
	git_vector *files;
} attr_walk_up_info;

static int push_one_attr(void *ref, git_buf *path)
{
	attr_walk_up_info *info = (attr_walk_up_info *)ref;
	return push_attrs(info->repo, info->files, path->ptr, GIT_ATTR_FILE);
}

static int collect_attr_files(
	git_repository *repo, const char *path, git_vector *files)
{
	int error;
	git_buf dir = GIT_BUF_INIT;
	const char *workdir = git_repository_workdir(repo);

	if (git_attr_cache__init(repo) < 0 ||
		git_vector_init(files, 4, NULL) < 0)
		return -1;

	error = git_path_find_dir(&dir, path, workdir);
	if (error < 0)
		goto cleanup;

	/* in precendence order highest to lowest:
	 * - $GIT_DIR/info/attributes
	 * - path components with .gitattributes up to the top level
	 *   of the worktree
	 * - config core.attributesfile
	 * - $GIT_PREFIX/etc/gitattributes
	 */

	error = push_attrs(
		repo, files, git_repository_path(repo), GIT_ATTR_FILE_INREPO);
	if (error < 0)
		goto cleanup;

	if (!git_repository_is_bare(repo)) {
		attr_walk_up_info info;

		info.repo = repo;
		info.files = files;
		error = git_path_walk_up(&dir, workdir, push_one_attr, &info);
		if (error < 0)
			goto cleanup;
	}

	if (git_repository_attr_cache(repo)->cfg_attr_file != NULL) {
		error = push_attrs(
			repo, files, NULL, git_repository_attr_cache(repo)->cfg_attr_file);
		if (error < 0)
			goto cleanup;
	}

	error = git_futils_find_system_file(&dir, GIT_ATTR_FILE_SYSTEM);
	if (!error)
		error = push_attrs(repo, files, NULL, dir.ptr);
	else if (error == GIT_ENOTFOUND)
		error = 0;

 cleanup:
	if (error < 0)
		git_vector_free(files);
	git_buf_free(&dir);

	return error;
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

	ret = git_config_get_string(cfg, GIT_ATTR_CONFIG, &cache->cfg_attr_file);
	if (ret < 0 && ret != GIT_ENOTFOUND)
		return ret;

	ret = git_config_get_string(cfg, GIT_IGNORE_CONFIG, &cache->cfg_excl_file);
	if (ret < 0 && ret != GIT_ENOTFOUND)
		return ret;

	giterr_clear();

	/* allocate hashtable for attribute and ignore file contents */
	if (cache->files == NULL) {
		cache->files = git_hashtable_alloc(
			8, git_hash__strhash_cb, git_hash__strcmp_cb);
		if (!cache->files)
			return -1;
	}

	/* allocate hashtable for attribute macros */
	if (cache->macros == NULL) {
		cache->macros = git_hashtable_alloc(
			8, git_hash__strhash_cb, git_hash__strcmp_cb);
		if (!cache->macros)
			return -1;
	}

	cache->initialized = 1;

	/* insert default macros */
	return git_attr_add_macro(repo, "binary", "-diff -crlf -text");
}

void git_attr_cache_flush(
	git_repository *repo)
{
	git_hashtable *table;

	if (!repo)
		return;

	if ((table = git_repository_attr_cache(repo)->files) != NULL) {
		git_attr_file *file;

		GIT_HASHTABLE_FOREACH_VALUE(table, file, git_attr_file__free(file));
		git_hashtable_free(table);

		git_repository_attr_cache(repo)->files = NULL;
	}

	if ((table = git_repository_attr_cache(repo)->macros) != NULL) {
		git_attr_rule *rule;

		GIT_HASHTABLE_FOREACH_VALUE(table, rule, git_attr_rule__free(rule));
		git_hashtable_free(table);

		git_repository_attr_cache(repo)->macros = NULL;
	}

	git_repository_attr_cache(repo)->initialized = 0;
}

int git_attr_cache__insert_macro(git_repository *repo, git_attr_rule *macro)
{
	/* TODO: generate warning log if (macro->assigns.length == 0) */
	if (macro->assigns.length == 0)
		return 0;

	return git_hashtable_insert(
		git_repository_attr_cache(repo)->macros, macro->match.pattern, macro);
}
