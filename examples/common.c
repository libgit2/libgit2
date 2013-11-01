/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"

void check_lg2(int error, const char *message, const char *extra)
{
	const git_error *lg2err;
	const char *lg2msg = "", *lg2spacer = "";

	if (!error)
		return;

	if ((lg2err = giterr_last()) != NULL && lg2err->message != NULL) {
		lg2msg = lg2err->message;
		lg2spacer = " - ";
	}

	if (extra)
		fprintf(stderr, "%s '%s' [%d]%s%s\n",
			message, extra, error, lg2spacer, lg2msg);
	else
		fprintf(stderr, "%s [%d]%s%s\n",
			message, error, lg2spacer, lg2msg);

	exit(1);
}

void fatal(const char *message, const char *extra)
{
	if (extra)
		fprintf(stderr, "%s %s\n", message, extra);
	else
		fprintf(stderr, "%s\n", message);

	exit(1);
}

size_t is_prefixed(const char *str, const char *pfx)
{
	size_t len = strlen(pfx);
	return strncmp(str, pfx, len) ? 0 : len;
}

int match_str_arg(
	const char **out, struct args_info *args, const char *opt)
{
	const char *found = args->argv[args->pos];
	size_t len = is_prefixed(found, opt);

	if (!len)
		return 0;

	if (!found[len]) {
		if (args->pos + 1 == args->argc)
			fatal("expected value following argument", opt);
		args->pos += 1;
		*out = args->argv[args->pos];
		return 1;
	}

	if (found[len] == '=') {
		*out = found + len + 1;
		return 1;
	}

	return 0;
}

static const char *match_numeric_arg(struct args_info *args, const char *opt)
{
	const char *found = args->argv[args->pos];
	size_t len = is_prefixed(found, opt);

	if (!len)
		return NULL;

	if (!found[len]) {
		if (args->pos + 1 == args->argc)
			fatal("expected numeric value following argument", opt);
		args->pos += 1;
		found = args->argv[args->pos];
	} else {
		found = found + len;
		if (*found == '=')
			found++;
	}

	return found;
}

int match_uint16_arg(
	uint16_t *out, struct args_info *args, const char *opt)
{
	const char *found = match_numeric_arg(args, opt);
	uint16_t val;
	char *endptr = NULL;

	if (!found)
		return 0;

	val = (uint16_t)strtoul(found, &endptr, 0);
	if (!endptr || *endptr != '\0')
		fatal("expected number after argument", opt);

	if (out)
		*out = val;
	return 1;
}

static int match_int_internal(
	int *out, const char *str, int allow_negative, const char *opt)
{
	char *endptr = NULL;
	int	  val = (int)strtol(str, &endptr, 10);

	if (!endptr || *endptr != '\0')
		fatal("expected number", opt);
	else if (val < 0 && !allow_negative)
		fatal("negative values are not allowed", opt);

	if (out)
		*out = val;

	return 1;
}

int is_integer(int *out, const char *str, int allow_negative)
{
	return match_int_internal(out, str, allow_negative, NULL);
}

int match_int_arg(
	int *out, struct args_info *args, const char *opt, int allow_negative)
{
	const char *found = match_numeric_arg(args, opt);
	if (!found)
		return 0;
	return match_int_internal(out, found, allow_negative, opt);
}

int diff_output(
	const git_diff_delta *d,
	const git_diff_hunk *h,
	const git_diff_line *l,
	void *p)
{
	FILE *fp = p;

	(void)d; (void)h;

	if (!fp)
		fp = stdout;

	if (l->origin == GIT_DIFF_LINE_CONTEXT ||
		l->origin == GIT_DIFF_LINE_ADDITION ||
		l->origin == GIT_DIFF_LINE_DELETION)
		fputc(l->origin, fp);

	fwrite(l->content, 1, l->content_len, fp);

	return 0;
}

void treeish_to_tree(
	git_tree **out, git_repository *repo, const char *treeish)
{
	git_object *obj = NULL;

	check_lg2(
		git_revparse_single(&obj, repo, treeish),
		"looking up object", treeish);

	check_lg2(
		git_object_peel((git_object **)out, obj, GIT_OBJ_TREE),
		"resolving object to tree", treeish);

	git_object_free(obj);
}

