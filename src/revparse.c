/*
 * Copyright (C) 2009-2012 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <assert.h>

#include "common.h"
#include "buffer.h"
#include "tree.h"

#include "git2.h"

typedef enum {
	REVPARSE_STATE_INIT,
	REVPARSE_STATE_CARET,
	REVPARSE_STATE_LINEAR,
	REVPARSE_STATE_COLON,
	REVPARSE_STATE_DONE,
} revparse_state;

static int revspec_error(const char *revspec)
{
	giterr_set(GITERR_INVALID, "Failed to parse revision specifier - Invalid pattern '%s'", revspec);
	return -1;
}

static int revparse_lookup_fully_qualifed_ref(git_object **out, git_repository *repo, const char*spec)
{
	git_oid resolved;
	int error;

	if ((error = git_reference_name_to_oid(&resolved, repo, spec)) < 0)
		return error;

	return git_object_lookup(out, repo, &resolved, GIT_OBJ_ANY);
}

/* Returns non-zero if yes */
static int spec_looks_like_describe_output(const char *spec)
{
	regex_t regex;
	int regex_error, retcode;

	regex_error = regcomp(&regex, ".+-[0-9]+-g[0-9a-fA-F]+", REG_EXTENDED);
	if (regex_error != 0) {
		giterr_set_regex(&regex, regex_error);
		return regex_error;
	}

	retcode = regexec(&regex, spec, 0, NULL, 0);
	regfree(&regex);
	return retcode == 0;
}

static int disambiguate_refname(git_reference **out, git_repository *repo, const char *refname)
{
	int error, i;
	bool fallbackmode = true;
	git_reference *ref;
	git_buf refnamebuf = GIT_BUF_INIT, name = GIT_BUF_INIT;

	static const char* formatters[] = {
		"%s",
		"refs/%s",
		"refs/tags/%s",
		"refs/heads/%s",
		"refs/remotes/%s",
		"refs/remotes/%s/HEAD",
		NULL
	};

	if (*refname)
		git_buf_puts(&name, refname);
	else {
		git_buf_puts(&name, "HEAD");
		fallbackmode = false;
	}

	for (i = 0; formatters[i] && (fallbackmode || i == 0); i++) {

		git_buf_clear(&refnamebuf);

		if ((error = git_buf_printf(&refnamebuf, formatters[i], git_buf_cstr(&name))) < 0)
			goto cleanup;

		error = git_reference_lookup_resolved(&ref, repo, git_buf_cstr(&refnamebuf), -1);

		if (!error) {
			*out = ref;
			error = 0;
			goto cleanup;
		}

		if (error != GIT_ENOTFOUND)
			goto cleanup;
	}

cleanup:
	git_buf_free(&name);
	git_buf_free(&refnamebuf);
	return error;
}

extern int revparse_lookup_object(git_object **out, git_repository *repo, const char *spec);

static int maybe_describe(git_object**out, git_repository *repo, const char *spec)
{
	const char *substr;
	int match;

	/* "git describe" output; snip everything before/including "-g" */
	substr = strstr(spec, "-g");

	if (substr == NULL)
		return GIT_ENOTFOUND;
	
	if ((match = spec_looks_like_describe_output(spec)) < 0)
		return match;

	if (!match)
		return GIT_ENOTFOUND;

	return revparse_lookup_object(out, repo, substr+2);
}

static int maybe_sha_or_abbrev(git_object**out, git_repository *repo, const char *spec)
{
	git_oid oid;
	size_t speclen = strlen(spec);

	if (git_oid_fromstrn(&oid, spec, speclen) < 0)
		return GIT_ENOTFOUND;

	return git_object_lookup_prefix(out, repo, &oid, speclen, GIT_OBJ_ANY);
}

int revparse_lookup_object(git_object **out, git_repository *repo, const char *spec)
{
	int error;
	git_reference *ref;

	error = maybe_describe(out, repo, spec);
	if (!error)
		return 0;

	if (error < 0 && error != GIT_ENOTFOUND)
		return error;

	error = maybe_sha_or_abbrev(out, repo, spec);
	if (!error)
		return 0;

	if (error < 0 && error != GIT_ENOTFOUND)
		return error;

	error = disambiguate_refname(&ref, repo, spec);
	if (!error) {
		error = git_object_lookup(out, repo, git_reference_oid(ref), GIT_OBJ_ANY);
		git_reference_free(ref);
		return 0;
	}

	if (error < 0 && error != GIT_ENOTFOUND)
		return error;

	giterr_set(GITERR_REFERENCE, "Refspec '%s' not found.", spec);
	return GIT_ENOTFOUND;
}

static int all_chars_are_digits(const char *str, size_t len)
{
	size_t i = 0;

	for (i = 0; i < len; i++)
		if (!git__isdigit(str[i])) return 0;

	return 1;
}

static int walk_ref_history(git_object **out, git_repository *repo, const char *refspec, const char *reflogspec)
{
	git_reference *disambiguated = NULL;
	git_reflog *reflog = NULL;
	int n, retcode = GIT_ERROR;
	int i, refloglen;
	const git_reflog_entry *entry;
	git_buf buf = GIT_BUF_INIT;
	size_t refspeclen = strlen(refspec);
	size_t reflogspeclen = strlen(reflogspec);

	if (git__prefixcmp(reflogspec, "@{") != 0 ||
		git__suffixcmp(reflogspec, "}") != 0)
		return revspec_error(reflogspec);

	/* "@{-N}" form means walk back N checkouts. That means the HEAD log. */
	if (!git__prefixcmp(reflogspec, "@{-")) {
		regex_t regex;
		int regex_error;

		if (refspeclen > 0)
			return revspec_error(reflogspec);

		if (git__strtol32(&n, reflogspec+3, NULL, 10) < 0 || n < 1)
			return revspec_error(reflogspec);

		if (!git_reference_lookup(&disambiguated, repo, "HEAD")) {
			if (!git_reflog_read(&reflog, disambiguated)) {
				regex_error = regcomp(&regex, "checkout: moving from (.*) to .*", REG_EXTENDED);
				if (regex_error != 0) {
					giterr_set_regex(&regex, regex_error);
				} else {
					regmatch_t regexmatches[2];

					retcode = GIT_ENOTFOUND;

					refloglen = git_reflog_entrycount(reflog);
					for (i=refloglen-1; i >= 0; i--) {
						const char *msg;
						entry = git_reflog_entry_byindex(reflog, i);

						msg = git_reflog_entry_msg(entry);
						if (!regexec(&regex, msg, 2, regexmatches, 0)) {
							n--;
							if (!n) {
								git_buf_put(&buf, msg+regexmatches[1].rm_so, regexmatches[1].rm_eo - regexmatches[1].rm_so);
								retcode = revparse_lookup_object(out, repo, git_buf_cstr(&buf));
								break;
							}
						}
					}
					regfree(&regex);
				}
			}
		}
	} else {
		int date_error = 0, result;
		git_time_t timestamp;
		git_buf datebuf = GIT_BUF_INIT;

		result = disambiguate_refname(&disambiguated, repo, refspec);

		if (result < 0) {
			retcode = result;
			goto cleanup;
		}

		git_buf_put(&datebuf, reflogspec+2, reflogspeclen-3);
		date_error = git__date_parse(&timestamp, git_buf_cstr(&datebuf));

		/* @{u} or @{upstream} -> upstream branch, for a tracking branch. This is stored in the config. */
		if (!git__prefixcmp(git_reference_name(disambiguated), GIT_REFS_HEADS_DIR) &&
			(!strcmp(reflogspec, "@{u}") || !strcmp(reflogspec, "@{upstream}"))) {
			git_config *cfg;
			if (!git_repository_config(&cfg, repo)) {
				/* Is the ref a tracking branch? */
				const char *remote;
				git_buf_clear(&buf);
				git_buf_printf(&buf, "branch.%s.remote",
					git_reference_name(disambiguated) + strlen(GIT_REFS_HEADS_DIR));

				if (!git_config_get_string(&remote, cfg, git_buf_cstr(&buf))) {
					/* Yes. Find the first merge target name. */
					const char *mergetarget;
					git_buf_clear(&buf);
					git_buf_printf(&buf, "branch.%s.merge",
						git_reference_name(disambiguated) + strlen(GIT_REFS_HEADS_DIR));

					if (!git_config_get_string(&mergetarget, cfg, git_buf_cstr(&buf)) &&
						!git__prefixcmp(mergetarget, "refs/heads/")) {
							/* Success. Look up the target and fetch the object. */
							git_buf_clear(&buf);
							git_buf_printf(&buf, "refs/remotes/%s/%s", remote, mergetarget+11);
							retcode = revparse_lookup_fully_qualifed_ref(out, repo, git_buf_cstr(&buf));
					}
				}
				git_config_free(cfg);
			}
		}

		/* @{N} -> Nth prior value for the ref (from reflog) */
		else if (all_chars_are_digits(reflogspec+2, reflogspeclen-3) &&
			!git__strtol32(&n, reflogspec+2, NULL, 10) &&
			n <= 100000000) { /* Allow integer time */

				git_buf_puts(&buf, git_reference_name(disambiguated));

				if (n == 0)
					retcode = revparse_lookup_fully_qualifed_ref(out, repo, git_buf_cstr(&buf));
				else if (!git_reflog_read(&reflog, disambiguated)) {
						int numentries = git_reflog_entrycount(reflog);
						if (numentries < n + 1) {
							giterr_set(GITERR_REFERENCE, "Reflog for '%s' has only %d entries, asked for %d",
								git_buf_cstr(&buf), numentries, n);
							retcode = GIT_ENOTFOUND;
						} else {
							const git_reflog_entry *entry = git_reflog_entry_byindex(reflog, n);
							const git_oid *oid = git_reflog_entry_oidold(entry);
							retcode = git_object_lookup(out, repo, oid, GIT_OBJ_ANY);
						}
				}
		}

		else if (!date_error) {
			/* Ref as it was on a certain date */
			git_reflog *reflog;
			if (!git_reflog_read(&reflog, disambiguated)) {
				/* Keep walking until we find an entry older than the given date */
				int numentries = git_reflog_entrycount(reflog);
				int i;

				for (i = numentries - 1; i >= 0; i--) {
					const git_reflog_entry *entry = git_reflog_entry_byindex(reflog, i);
					git_time commit_time = git_reflog_entry_committer(entry)->when;
					if (commit_time.time - timestamp <= 0) {
						retcode = git_object_lookup(out, repo, git_reflog_entry_oidnew(entry), GIT_OBJ_ANY);
						break;
					}
				}

				if (i ==  -1) {
					/* Didn't find a match */
					retcode = GIT_ENOTFOUND;
				}

				git_reflog_free(reflog);
			}
		}

		git_buf_free(&datebuf);
	}

cleanup:
	if (reflog)
		git_reflog_free(reflog);
	git_buf_free(&buf);
	git_reference_free(disambiguated);
	return retcode;
}

static git_object* dereference_object(git_object *obj)
{
	git_otype type = git_object_type(obj);

	switch (type) {
	case GIT_OBJ_COMMIT:
		{
			git_tree *tree = NULL;
			if (0 == git_commit_tree(&tree, (git_commit*)obj)) {
				return (git_object*)tree;
			}
		}
		break;
	case GIT_OBJ_TAG:
		{
			git_object *newobj = NULL;
			if (0 == git_tag_target(&newobj, (git_tag*)obj)) {
				return newobj;
			}
		}
		break;

	default:
	case GIT_OBJ_TREE:
	case GIT_OBJ_BLOB:
	case GIT_OBJ_OFS_DELTA:
	case GIT_OBJ_REF_DELTA:
		break;
	}

	/* Can't dereference some types */
	return NULL;
}

static int dereference_to_type(git_object **out, git_object *obj, git_otype target_type)
{
	int retcode = 1;
	git_object *obj1 = obj, *obj2 = obj;

	while (retcode > 0) {
		git_otype this_type = git_object_type(obj1);

		if (this_type == target_type) {
			*out = obj1;
			retcode = 0;
		} else {
			/* Dereference once, if possible. */
			obj2 = dereference_object(obj1);
			if (!obj2) {
				giterr_set(GITERR_REFERENCE, "Can't dereference to type");
				retcode = GIT_ERROR;
			}
		}
		if (obj1 != obj && obj1 != obj2) {
			git_object_free(obj1);
		}
		obj1 = obj2;
	}
	return retcode;
}

static git_otype parse_obj_type(const char *str)
{
	if (!strcmp(str, "{commit}")) return GIT_OBJ_COMMIT;
	if (!strcmp(str, "{tree}")) return GIT_OBJ_TREE;
	if (!strcmp(str, "{blob}")) return GIT_OBJ_BLOB;
	if (!strcmp(str, "{tag}")) return GIT_OBJ_TAG;
	return GIT_OBJ_BAD;
}

static int handle_caret_syntax(git_object **out, git_repository *repo, git_object *obj, const char *movement)
{
	git_commit *commit;
	size_t movementlen = strlen(movement);
	int n;

	if (*movement == '{') {
		if (movement[movementlen-1] != '}')
			return revspec_error(movement);

		/* {} -> Dereference until we reach an object that isn't a tag. */
		if (movementlen == 2) {
			git_object *newobj = obj;
			git_object *newobj2 = newobj;
			while (git_object_type(newobj2) == GIT_OBJ_TAG) {
				newobj2 = dereference_object(newobj);
				if (newobj != obj) git_object_free(newobj);
				if (!newobj2) {
					giterr_set(GITERR_REFERENCE, "Couldn't find object of target type.");
					return GIT_ERROR;
				}
				newobj = newobj2;
			}
			*out = newobj2;
			return 0;
		}

		/* {/...} -> Walk all commits until we see a commit msg that matches the phrase. */
		if (movement[1] == '/') {
			int retcode = GIT_ERROR;
			git_revwalk *walk;
			if (!git_revwalk_new(&walk, repo)) {
				git_oid oid;
				regex_t preg;
				int reg_error;
				git_buf buf = GIT_BUF_INIT;

				git_revwalk_sorting(walk, GIT_SORT_TIME);
				git_revwalk_push(walk, git_object_id(obj));

				/* Extract the regex from the movement string */
				git_buf_put(&buf, movement+2, strlen(movement)-3);

				reg_error = regcomp(&preg, git_buf_cstr(&buf), REG_EXTENDED);
				if (reg_error != 0) {
					giterr_set_regex(&preg, reg_error);
				} else {
					while(!git_revwalk_next(&oid, walk)) {
						git_object *walkobj;

						/* Fetch the commit object, and check for matches in the message */
						if (!git_object_lookup(&walkobj, repo, &oid, GIT_OBJ_COMMIT)) {
							if (!regexec(&preg, git_commit_message((git_commit*)walkobj), 0, NULL, 0)) {
								/* Found it! */
								retcode = 0;
								*out = walkobj;
								if (obj == walkobj) {
									/* Avoid leaking an object */
									git_object_free(walkobj);
								}
								break;
							}
							git_object_free(walkobj);
						}
					}
					if (retcode < 0) {
						giterr_set(GITERR_REFERENCE, "Couldn't find a match for %s", movement);
					}
					regfree(&preg);
				}

				git_buf_free(&buf);
				git_revwalk_free(walk);
			}
			return retcode;
		}

		/* {...} -> Dereference until we reach an object of a certain type. */
		if (dereference_to_type(out, obj, parse_obj_type(movement)) < 0) {
			return GIT_ERROR;
		}
		return 0;
	}

	/* Dereference until we reach a commit. */
	if (dereference_to_type(&obj, obj, GIT_OBJ_COMMIT) < 0) {
		/* Can't dereference to a commit; fail */
		return GIT_ERROR;
	}

	/* "^" is the same as "^1" */
	if (movementlen == 0) {
		n = 1;
	} else {
		git__strtol32(&n, movement, NULL, 0);
	}
	commit = (git_commit*)obj;

	/* "^0" just returns the input */
	if (n == 0) {
		*out = obj;
		return 0;
	}

	if (git_commit_parent(&commit, commit, n-1) < 0) {
		return GIT_ENOTFOUND;
	}

	*out = (git_object*)commit;
	return 0;
}

static int handle_linear_syntax(git_object **out, git_object *obj, const char *movement)
{
	git_commit *commit1, *commit2;
	int i, n;

	/* Dereference until we reach a commit. */
	if (dereference_to_type(&obj, obj, GIT_OBJ_COMMIT) < 0) {
		/* Can't dereference to a commit; fail */
		return GIT_ERROR;
	}

	/* "~" is the same as "~1" */
	if (*movement == '\0') {
		n = 1;
	} else if (git__strtol32(&n, movement, NULL, 0) < 0) {
		return GIT_ERROR;
	}
	commit1 = (git_commit*)obj;

	/* "~0" just returns the input */
	if (n == 0) {
		*out = obj;
		return 0;
	}

	for (i=0; i<n; i++) {
		if (git_commit_parent(&commit2, commit1, 0) < 0) {
			return GIT_ERROR;
		}
		if (commit1 != (git_commit*)obj) {
			git_commit_free(commit1);
		}
		commit1 = commit2;
	}

	*out = (git_object*)commit1;
	return 0;
}

static int oid_for_tree_path(git_oid *out, git_tree *tree, git_repository *repo, const char *path)
{
	char *str, *tok;
	void *alloc;
	git_tree *tree2 = tree;
	const git_tree_entry *entry = NULL;
	git_otype type;

	if (*path == '\0') {
		git_oid_cpy(out, git_object_id((git_object *)tree));
		return 0;
	}

	alloc = str = git__strdup(path);

	while ((tok = git__strtok(&str, "/\\")) != NULL) {
		entry = git_tree_entry_byname(tree2, tok);
		if (tree2 != tree) git_tree_free(tree2);

		if (entry == NULL)
			break;

		type = git_tree_entry_type(entry);

		switch (type) {
		case GIT_OBJ_TREE:
			if (*str == '\0')
				break;
			if (git_tree_lookup(&tree2, repo, &entry->oid) < 0) {
				git__free(alloc);
				return GIT_ERROR;
			}
			break;
		case GIT_OBJ_BLOB:
			if (*str != '\0') {
				entry = NULL;
				goto out;
			}
			break;
		default:
			/* TODO: support submodules? */
			giterr_set(GITERR_INVALID, "Unimplemented");
			git__free(alloc);
			return GIT_ERROR;
		}
	}

out:
	if (!entry) {
		giterr_set(GITERR_INVALID, "Invalid tree path '%s'", path);
		git__free(alloc);
		return GIT_ENOTFOUND;
	}

	git_oid_cpy(out, git_tree_entry_id(entry));
	git__free(alloc);
	return 0;
}

static int handle_colon_syntax(git_object **out,
	git_repository *repo,
	git_object *obj,
	const char *path)
{
	git_tree *tree;
	git_oid oid;
	int error;

	/* Dereference until we reach a tree. */
	if (dereference_to_type(&obj, obj, GIT_OBJ_TREE) < 0) {
		return GIT_ERROR;
	}
	tree = (git_tree*)obj;

	/* Find the blob or tree at the given path. */
	error = oid_for_tree_path(&oid, tree, repo, path);
	git_tree_free(tree);

	if (error < 0)
		return error;

	return git_object_lookup(out, repo, &oid, GIT_OBJ_ANY);
}

static int revparse_global_grep(git_object **out, git_repository *repo, const char *pattern)
{
	git_revwalk *walk;
	int retcode = GIT_ERROR;

	if (!pattern[0]) {
		giterr_set(GITERR_REGEX, "Empty pattern");
		return GIT_ERROR;
	}

	if (!git_revwalk_new(&walk, repo)) {
		regex_t preg;
		int reg_error;
		git_oid oid;

		git_revwalk_sorting(walk, GIT_SORT_TIME);
		git_revwalk_push_glob(walk, "refs/heads/*");

		reg_error = regcomp(&preg, pattern, REG_EXTENDED);
		if (reg_error != 0) {
			giterr_set_regex(&preg, reg_error);
		} else {
			git_object *walkobj = NULL, *resultobj = NULL;
			while(!git_revwalk_next(&oid, walk)) {
				/* Fetch the commit object, and check for matches in the message */
				if (walkobj != resultobj) git_object_free(walkobj);
				if (!git_object_lookup(&walkobj, repo, &oid, GIT_OBJ_COMMIT)) {
					if (!regexec(&preg, git_commit_message((git_commit*)walkobj), 0, NULL, 0)) {
						/* Match! */
						resultobj = walkobj;
						retcode = 0;
						break;
					}
				}
			}
			if (!resultobj) {
				giterr_set(GITERR_REFERENCE, "Couldn't find a match for %s", pattern);
				retcode = GIT_ENOTFOUND;
				git_object_free(walkobj);
			} else {
				*out = resultobj;
			}
			regfree(&preg);
			git_revwalk_free(walk);
		}
	}

	return retcode;
}

int git_revparse_single(git_object **out, git_repository *repo, const char *spec)
{
	revparse_state current_state = REVPARSE_STATE_INIT,  next_state = REVPARSE_STATE_INIT;
	const char *spec_cur = spec;
	git_object *cur_obj = NULL,  *next_obj = NULL;
	git_buf specbuffer = GIT_BUF_INIT,  stepbuffer = GIT_BUF_INIT;
	int retcode = 0;

	assert(out && repo && spec);

	if (spec[0] == ':') {
		if (spec[1] == '/') {
			return revparse_global_grep(out, repo, spec+2);
		}
		/* TODO: support merge-stage path lookup (":2:Makefile"). */
		giterr_set(GITERR_INVALID, "Unimplemented");
		return GIT_ERROR;
	}

	while (current_state != REVPARSE_STATE_DONE) {
		switch (current_state) {
		case REVPARSE_STATE_INIT:
			if (!*spec_cur) {
				/* No operators, just a name. Find it and return. */
				retcode = revparse_lookup_object(out, repo, spec);
				next_state = REVPARSE_STATE_DONE;
			} else if (*spec_cur == '@') {
				/* '@' syntax doesn't allow chaining */
				git_buf_puts(&stepbuffer, spec_cur);
				retcode = walk_ref_history(out, repo, git_buf_cstr(&specbuffer), git_buf_cstr(&stepbuffer));
				next_state = REVPARSE_STATE_DONE;
			} else if (*spec_cur == '^') {
				next_state = REVPARSE_STATE_CARET;
			} else if (*spec_cur == '~') {
				next_state = REVPARSE_STATE_LINEAR;
			} else if (*spec_cur == ':') {
				next_state = REVPARSE_STATE_COLON;
			} else {
				git_buf_putc(&specbuffer, *spec_cur);
			}
			spec_cur++;

			if (current_state != next_state && next_state != REVPARSE_STATE_DONE) {
				/* Leaving INIT state, find the object specified, in case that state needs it */
				if ((retcode = revparse_lookup_object(&next_obj, repo, git_buf_cstr(&specbuffer))) < 0)
					next_state = REVPARSE_STATE_DONE;
			}
			break;


		case REVPARSE_STATE_CARET:
			/* Gather characters until NULL, '~', or '^' */
			if (!*spec_cur) {
				retcode = handle_caret_syntax(out, repo, cur_obj, git_buf_cstr(&stepbuffer));
				next_state = REVPARSE_STATE_DONE;
			} else if (*spec_cur == '~') {
				retcode = handle_caret_syntax(&next_obj, repo, cur_obj, git_buf_cstr(&stepbuffer));
				git_buf_clear(&stepbuffer);
				next_state = !retcode ? REVPARSE_STATE_LINEAR : REVPARSE_STATE_DONE;
			} else if (*spec_cur == '^') {
				retcode = handle_caret_syntax(&next_obj, repo, cur_obj, git_buf_cstr(&stepbuffer));
				git_buf_clear(&stepbuffer);
				if (retcode < 0) {
					next_state = REVPARSE_STATE_DONE;
				}
			} else if (*spec_cur == ':') {
				retcode = handle_caret_syntax(&next_obj, repo, cur_obj, git_buf_cstr(&stepbuffer));
				git_buf_clear(&stepbuffer);
				next_state = !retcode ? REVPARSE_STATE_COLON : REVPARSE_STATE_DONE;
			} else {
				git_buf_putc(&stepbuffer, *spec_cur);
			}
			spec_cur++;
			break;

		case REVPARSE_STATE_LINEAR:
			if (!*spec_cur) {
				retcode = handle_linear_syntax(out, cur_obj, git_buf_cstr(&stepbuffer));
				next_state = REVPARSE_STATE_DONE;
			} else if (*spec_cur == '~') {
				retcode = handle_linear_syntax(&next_obj, cur_obj, git_buf_cstr(&stepbuffer));
				git_buf_clear(&stepbuffer);
				if (retcode < 0) {
					next_state = REVPARSE_STATE_DONE;
				}
			} else if (*spec_cur == '^') {
				retcode = handle_linear_syntax(&next_obj, cur_obj, git_buf_cstr(&stepbuffer));
				git_buf_clear(&stepbuffer);
				next_state = !retcode ? REVPARSE_STATE_CARET : REVPARSE_STATE_DONE;
			} else {
				git_buf_putc(&stepbuffer, *spec_cur);
			}
			spec_cur++;
			break;

		case REVPARSE_STATE_COLON:
			if (*spec_cur) {
				git_buf_putc(&stepbuffer, *spec_cur);
			} else {
				retcode = handle_colon_syntax(out, repo, cur_obj, git_buf_cstr(&stepbuffer));
				next_state = REVPARSE_STATE_DONE;
			}
			spec_cur++;
			break;

		case REVPARSE_STATE_DONE:
			if (cur_obj && *out != cur_obj) git_object_free(cur_obj);
			if (next_obj && *out != next_obj) git_object_free(next_obj);
			break;
		}

		current_state = next_state;
		if (cur_obj != next_obj) {
			if (cur_obj) git_object_free(cur_obj);
			cur_obj = next_obj;
		}
	}

	if (*out != cur_obj) git_object_free(cur_obj);
	if (*out != next_obj && next_obj != cur_obj) git_object_free(next_obj);

	git_buf_free(&specbuffer);
	git_buf_free(&stepbuffer);
	return retcode;
}
