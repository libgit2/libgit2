#include "common.h"
#include "repository.h"
#include "filebuf.h"
#include <ctype.h>

const char *git_attr__true  = "[internal]__TRUE__";
const char *git_attr__false = "[internal]__FALSE__";

static int sort_by_hash_and_name(const void *a_raw, const void *b_raw);
static void git_attr_rule__clear(git_attr_rule *rule);

int git_attr_file__new(git_attr_file **attrs_ptr)
{
	int error;
	git_attr_file *attrs = NULL;

	attrs = git__calloc(1, sizeof(git_attr_file));
	if (attrs == NULL)
		error = GIT_ENOMEM;
	else
		error = git_vector_init(&attrs->rules, 4, NULL);

	if (error != GIT_SUCCESS) {
		git__rethrow(error, "Could not allocate attribute storage");
		git__free(attrs);
		attrs = NULL;
	}

	*attrs_ptr = attrs;

	return error;
}

int git_attr_file__set_path(
	git_repository *repo, const char *path, git_attr_file *file)
{
	if (file->path != NULL) {
		git__free(file->path);
		file->path = NULL;
	}

	if (repo == NULL)
		file->path = git__strdup(path);
	else {
		const char *workdir = git_repository_workdir(repo);

		if (workdir && git__prefixcmp(path, workdir) == 0)
			file->path = git__strdup(path + strlen(workdir));
		else
			file->path = git__strdup(path);
	}

	return (file->path == NULL) ? GIT_ENOMEM : GIT_SUCCESS;
}

int git_attr_file__from_buffer(
	git_repository *repo, const char *buffer, git_attr_file *attrs)
{
	int error = GIT_SUCCESS;
	const char *scan = NULL;
	char *context = NULL;
	git_attr_rule *rule = NULL;

	assert(buffer && attrs);

	scan = buffer;

	if (attrs->path && git__suffixcmp(attrs->path, GIT_ATTR_FILE) == 0) {
		context = git__strndup(attrs->path,
			strlen(attrs->path) - strlen(GIT_ATTR_FILE));
		if (!context) error = GIT_ENOMEM;
	}

	while (error == GIT_SUCCESS && *scan) {
		/* allocate rule if needed */
		if (!rule && !(rule = git__calloc(1, sizeof(git_attr_rule)))) {
			error = GIT_ENOMEM;
			break;
		}

		/* parse the next "pattern attr attr attr" line */
		if (!(error = git_attr_fnmatch__parse(&rule->match, context, &scan)) &&
			!(error = git_attr_assignment__parse(repo, &rule->assigns, &scan)))
		{
			if (rule->match.flags & GIT_ATTR_FNMATCH_MACRO)
				/* should generate error/warning if this is coming from any
				 * file other than .gitattributes at repo root.
				 */
				error = git_attr_cache__insert_macro(repo, rule);
			else
				error = git_vector_insert(&attrs->rules, rule);
		}

		/* if the rule wasn't a pattern, on to the next */
		if (error != GIT_SUCCESS) {
			git_attr_rule__clear(rule); /* reset rule contents */
			if (error == GIT_ENOTFOUND)
				error = GIT_SUCCESS;
		} else {
			rule = NULL; /* vector now "owns" the rule */
		}
	}

	git_attr_rule__free(rule);
	git__free(context);

	return error;
}

int git_attr_file__from_file(
	git_repository *repo, const char *path, git_attr_file *file)
{
	int error = GIT_SUCCESS;
	git_buf fbuf = GIT_BUF_INIT;

	assert(path && file);

	if (file->path == NULL)
		error = git_attr_file__set_path(repo, path, file);

	if (error == GIT_SUCCESS &&
		(error = git_futils_readbuffer(&fbuf, path)) == GIT_SUCCESS)
		error = git_attr_file__from_buffer(repo, fbuf.ptr, file);

	git_buf_free(&fbuf);
	if (error != GIT_SUCCESS)
		git__rethrow(error, "Could not open attribute file '%s'", path);

	return error;
}

void git_attr_file__free(git_attr_file *file)
{
	unsigned int i;
	git_attr_rule *rule;

	if (!file)
		return;

	git_vector_foreach(&file->rules, i, rule)
		git_attr_rule__free(rule);

	git_vector_free(&file->rules);

	git__free(file->path);
	file->path = NULL;

	git__free(file);
}

unsigned long git_attr_file__name_hash(const char *name)
{
	unsigned long h = 5381;
	int c;
	assert(name);
	while ((c = (int)*name++) != 0)
		h = ((h << 5) + h) + c;
	return h;
}


int git_attr_file__lookup_one(
	git_attr_file *file,
	const git_attr_path *path,
	const char *attr,
	const char **value)
{
	unsigned int i;
	git_attr_name name;
	git_attr_rule *rule;

	*value = NULL;

	name.name = attr;
	name.name_hash = git_attr_file__name_hash(attr);

	git_attr_file__foreach_matching_rule(file, path, i, rule) {
		int pos = git_vector_bsearch(&rule->assigns, &name);
		git_clearerror(); /* okay if search failed */

		if (pos >= 0) {
			*value = ((git_attr_assignment *)
					  git_vector_get(&rule->assigns, pos))->value;
			break;
		}
	}

	return GIT_SUCCESS;
}


int git_attr_fnmatch__match(
	git_attr_fnmatch *match,
	const git_attr_path *path)
{
	int matched = FNM_NOMATCH;

	if (match->flags & GIT_ATTR_FNMATCH_DIRECTORY && !path->is_dir)
		return matched;

	if (match->flags & GIT_ATTR_FNMATCH_FULLPATH)
		matched = p_fnmatch(match->pattern, path->path, FNM_PATHNAME);
	else if (path->is_dir)
		matched = p_fnmatch(match->pattern, path->basename, FNM_LEADING_DIR);
	else
		matched = p_fnmatch(match->pattern, path->basename, 0);

	return matched;
}

int git_attr_rule__match(
	git_attr_rule *rule,
	const git_attr_path *path)
{
	int matched = git_attr_fnmatch__match(&rule->match, path);

	if (rule->match.flags & GIT_ATTR_FNMATCH_NEGATIVE)
		matched = (matched == GIT_SUCCESS) ? FNM_NOMATCH : GIT_SUCCESS;

	return matched;
}


git_attr_assignment *git_attr_rule__lookup_assignment(
	git_attr_rule *rule, const char *name)
{
	int pos;
	git_attr_name key;
	key.name = name;
	key.name_hash = git_attr_file__name_hash(name);

	pos = git_vector_bsearch(&rule->assigns, &key);
	git_clearerror(); /* okay if search failed */

	return (pos >= 0) ? git_vector_get(&rule->assigns, pos) : NULL;
}

int git_attr_path__init(
	git_attr_path *info, const char *path, const char *base)
{
	assert(info && path);
	info->path = path;
	info->basename = strrchr(path, '/');
	if (info->basename)
		info->basename++;
	if (!info->basename || !*info->basename)
		info->basename = path;

	if (base != NULL && git_path_root(path) < 0) {
		git_buf full_path = GIT_BUF_INIT;
		int error = git_buf_joinpath(&full_path, base, path);
		if (error == GIT_SUCCESS)
			info->is_dir = (git_path_isdir(full_path.ptr) == GIT_SUCCESS);
		git_buf_free(&full_path);
		return error;
	}
	info->is_dir = (git_path_isdir(path) == GIT_SUCCESS);

	return GIT_SUCCESS;
}


/*
 * From gitattributes(5):
 *
 * Patterns have the following format:
 *
 * - A blank line matches no files, so it can serve as a separator for
 *   readability.
 *
 * - A line starting with # serves as a comment.
 *
 * - An optional prefix ! which negates the pattern; any matching file
 *   excluded by a previous pattern will become included again. If a negated
 *   pattern matches, this will override lower precedence patterns sources.
 *
 * - If the pattern ends with a slash, it is removed for the purpose of the
 *   following description, but it would only find a match with a directory. In
 *   other words, foo/ will match a directory foo and paths underneath it, but
 *   will not match a regular file or a symbolic link foo (this is consistent
 *   with the way how pathspec works in general in git).
 *
 * - If the pattern does not contain a slash /, git treats it as a shell glob
 *   pattern and checks for a match against the pathname without leading
 *   directories.
 *
 * - Otherwise, git treats the pattern as a shell glob suitable for consumption
 *   by fnmatch(3) with the FNM_PATHNAME flag: wildcards in the pattern will
 *   not match a / in the pathname. For example, "Documentation/\*.html" matches
 *   "Documentation/git.html" but not "Documentation/ppc/ppc.html". A leading
 *   slash matches the beginning of the pathname; for example, "/\*.c" matches
 *   "cat-file.c" but not "mozilla-sha1/sha1.c".
 */

/*
 * This will return GIT_SUCCESS if the spec was filled out,
 * GIT_ENOTFOUND if the fnmatch does not require matching, or
 * another error code there was an actual problem.
 */
int git_attr_fnmatch__parse(
	git_attr_fnmatch *spec,
	const char *source,
	const char **base)
{
	const char *pattern, *scan;
	int slash_count;

	assert(spec && base && *base);

	pattern = *base;

	while (isspace(*pattern)) pattern++;
	if (!*pattern || *pattern == '#') {
		*base = git__next_line(pattern);
		return GIT_ENOTFOUND;
	}

	spec->flags = 0;

	if (*pattern == '[') {
		if (strncmp(pattern, "[attr]", 6) == 0) {
			spec->flags = spec->flags | GIT_ATTR_FNMATCH_MACRO;
			pattern += 6;
		}
		/* else a character range like [a-e]* which is accepted */
	}

	if (*pattern == '!') {
		spec->flags = spec->flags | GIT_ATTR_FNMATCH_NEGATIVE;
		pattern++;
	}

	slash_count = 0;
	for (scan = pattern; *scan != '\0'; ++scan) {
		/* scan until (non-escaped) white space */
		if (isspace(*scan) && *(scan - 1) != '\\')
			break;

		if (*scan == '/') {
			spec->flags = spec->flags | GIT_ATTR_FNMATCH_FULLPATH;
			slash_count++;
		}
	}

	*base = scan;

	spec->length = scan - pattern;

	if (pattern[spec->length - 1] == '/') {
		spec->length--;
		spec->flags = spec->flags | GIT_ATTR_FNMATCH_DIRECTORY;
		if (--slash_count <= 0)
			spec->flags = spec->flags & ~GIT_ATTR_FNMATCH_FULLPATH;
	}

	if ((spec->flags & GIT_ATTR_FNMATCH_FULLPATH) != 0 &&
		source != NULL && git_path_root(pattern) < 0)
	{
		size_t sourcelen = strlen(source);
		/* given an unrooted fullpath match from a file inside a repo,
		 * prefix the pattern with the relative directory of the source file
		 */
		spec->pattern = git__malloc(sourcelen + spec->length + 1);
		if (spec->pattern) {
			memcpy(spec->pattern, source, sourcelen);
			memcpy(spec->pattern + sourcelen, pattern, spec->length);
			spec->length += sourcelen;
			spec->pattern[spec->length] = '\0';
		}
	} else {
		spec->pattern = git__strndup(pattern, spec->length);
	}

	if (!spec->pattern) {
		*base = git__next_line(pattern);
		return GIT_ENOMEM;
	} else {
		/* strip '\' that might have be used for internal whitespace */
		char *to = spec->pattern;
		for (scan = spec->pattern; *scan; to++, scan++) {
			if (*scan == '\\')
				scan++; /* skip '\' but include next char */
			if (to != scan)
				*to = *scan;
		}
		if (to != scan) {
			*to = '\0';
			spec->length = (to - spec->pattern);
		}
	}

	return GIT_SUCCESS;
}

static int sort_by_hash_and_name(const void *a_raw, const void *b_raw)
{
	const git_attr_name *a = a_raw;
	const git_attr_name *b = b_raw;

	if (b->name_hash < a->name_hash)
		return 1;
	else if (b->name_hash > a->name_hash)
		return -1;
	else
		return strcmp(b->name, a->name);
}

static void git_attr_assignment__free(git_attr_assignment *assign)
{
	git__free(assign->name);
	assign->name = NULL;

	if (assign->is_allocated) {
		git__free((void *)assign->value);
		assign->value = NULL;
	}

	git__free(assign);
}

static int merge_assignments(void **old_raw, void *new_raw)
{
	git_attr_assignment **old = (git_attr_assignment **)old_raw;
	git_attr_assignment *new = (git_attr_assignment *)new_raw;

	GIT_REFCOUNT_DEC(*old, git_attr_assignment__free);
	*old = new;
	return GIT_EEXISTS;
}

int git_attr_assignment__parse(
	git_repository *repo,
	git_vector *assigns,
	const char **base)
{
	int error = GIT_SUCCESS;
	const char *scan = *base;
	git_attr_assignment *assign = NULL;

	assert(assigns && !assigns->length);

	assigns->_cmp = sort_by_hash_and_name;

	while (*scan && *scan != '\n' && error == GIT_SUCCESS) {
		const char *name_start, *value_start;

		/* skip leading blanks */
		while (isspace(*scan) && *scan != '\n') scan++;

		/* allocate assign if needed */
		if (!assign) {
			assign = git__calloc(1, sizeof(git_attr_assignment));
			if (!assign) {
				error = GIT_ENOMEM;
				break;
			}
			GIT_REFCOUNT_INC(assign);
		}

		assign->name_hash = 5381;
		assign->value = git_attr__true;
		assign->is_allocated = 0;

		/* look for magic name prefixes */
		if (*scan == '-') {
			assign->value = git_attr__false;
			scan++;
		} else if (*scan == '!') {
			assign->value = NULL; /* explicit unspecified state */
			scan++;
		} else if (*scan == '#') /* comment rest of line */
			break;

		/* find the name */
		name_start = scan;
		while (*scan && !isspace(*scan) && *scan != '=') {
			assign->name_hash =
				((assign->name_hash << 5) + assign->name_hash) + *scan;
			scan++;
		}
		if (scan == name_start) {
			/* must have found lone prefix (" - ") or leading = ("=foo")
			 * or end of buffer -- advance until whitespace and continue
			 */
			while (*scan && !isspace(*scan)) scan++;
			continue;
		}

		/* allocate permanent storage for name */
		assign->name = git__strndup(name_start, scan - name_start);
		if (!assign->name) {
			error = GIT_ENOMEM;
			break;
		}

		/* if there is an equals sign, find the value */
		if (*scan == '=') {
			for (value_start = ++scan; *scan && !isspace(*scan); ++scan);

			/* if we found a value, allocate permanent storage for it */
			if (scan > value_start) {
				assign->value = git__strndup(value_start, scan - value_start);
				if (!assign->value) {
					error = GIT_ENOMEM;
					break;
				} else {
					assign->is_allocated = 1;
				}
			}
		}

		/* expand macros (if given a repo with a macro cache) */
		if (repo != NULL && assign->value == git_attr__true) {
			git_attr_rule *macro =
				git_hashtable_lookup(repo->attrcache.macros, assign->name);

			if (macro != NULL) {
				unsigned int i;
				git_attr_assignment *massign;

				git_vector_foreach(&macro->assigns, i, massign) {
					GIT_REFCOUNT_INC(massign);

					error = git_vector_insert_sorted(
						assigns, massign, &merge_assignments);

					if (error == GIT_EEXISTS)
						error = GIT_SUCCESS;
					else if (error != GIT_SUCCESS)
						break;
				}
			}
		}

		/* insert allocated assign into vector */
		error = git_vector_insert_sorted(assigns, assign, &merge_assignments);
		if (error == GIT_EEXISTS)
			error = GIT_SUCCESS;
		else if (error < GIT_SUCCESS)
			break;

		/* clear assign since it is now "owned" by the vector */
		assign = NULL;
	}

	if (!assigns->length)
		error = git__throw(GIT_ENOTFOUND, "No attribute assignments found for rule");

	if (assign != NULL)
		git_attr_assignment__free(assign);

	*base = git__next_line(scan);

	return error;
}

static void git_attr_rule__clear(git_attr_rule *rule)
{
	unsigned int i;
	git_attr_assignment *assign;

	if (!rule)
		return;

	if (!(rule->match.flags & GIT_ATTR_FNMATCH_IGNORE)) {
		git_vector_foreach(&rule->assigns, i, assign)
			GIT_REFCOUNT_DEC(assign, git_attr_assignment__free);
		git_vector_free(&rule->assigns);
	}

	git__free(rule->match.pattern);
	rule->match.pattern = NULL;
	rule->match.length = 0;
}

void git_attr_rule__free(git_attr_rule *rule)
{
	git_attr_rule__clear(rule);
	git__free(rule);
}

