#include "git2/patch.h"
#include "diff_patch.h"

#define parse_err(...) \
	( giterr_set(GITERR_PATCH, __VA_ARGS__), -1 )

typedef struct {
	const char *content;
	size_t content_len;

	const char *line;
	size_t line_len;
	size_t line_num;

	size_t remain;

	char *header_new_path;
	char *header_old_path;
} patch_parse_ctx;


static void parse_advance_line(patch_parse_ctx *ctx)
{
	ctx->line += ctx->line_len;
	ctx->remain -= ctx->line_len;
	ctx->line_len = git__linenlen(ctx->line, ctx->remain);
	ctx->line_num++;
}

static void parse_advance_chars(patch_parse_ctx *ctx, size_t char_cnt)
{
	ctx->line += char_cnt;
	ctx->remain -= char_cnt;
	ctx->line_len -= char_cnt;
}

static int parse_advance_expected(
	patch_parse_ctx *ctx,
	const char *expected,
	size_t expected_len)
{
	if (ctx->line_len < expected_len)
		return -1;

	if (memcmp(ctx->line, expected, expected_len) != 0)
		return -1;

	parse_advance_chars(ctx, expected_len);
	return 0;
}

static int parse_advance_ws(patch_parse_ctx *ctx)
{
	int ret = -1;

	while (ctx->line_len > 0 &&
		ctx->line[0] != '\n' &&
		git__isspace(ctx->line[0])) {
		ctx->line++;
		ctx->line_len--;
		ctx->remain--;
		ret = 0;
	}

	return ret;
}

static int parse_advance_nl(patch_parse_ctx *ctx)
{
	if (ctx->line_len != 1 || ctx->line[0] != '\n')
		return -1;

	parse_advance_line(ctx);
	return 0;
}

static int header_path_len(patch_parse_ctx *ctx)
{
	bool inquote = 0;
	bool quoted = (ctx->line_len > 0 && ctx->line[0] == '"');
	size_t len;

	for (len = quoted; len < ctx->line_len; len++) {
		if (!quoted && git__isspace(ctx->line[len]))
			break;
		else if (quoted && !inquote && ctx->line[len] == '"') {
			len++;
			break;
		}

		inquote = (!inquote && ctx->line[len] == '\\');
	}

	return len;
}

static int parse_header_path_buf(git_buf *path, patch_parse_ctx *ctx)
{
	int path_len, error = 0;

	path_len = header_path_len(ctx);

	if ((error = git_buf_put(path, ctx->line, path_len)) < 0)
		goto done;

	parse_advance_chars(ctx, path_len);

	git_buf_rtrim(path);

	if (path->size > 0 && path->ptr[0] == '"')
		error = git_buf_unquote(path);

	if (error < 0)
		goto done;

	git_path_squash_slashes(path);

done:
	return error;
}

static int parse_header_path(char **out, patch_parse_ctx *ctx)
{
	git_buf path = GIT_BUF_INIT;
	int error = parse_header_path_buf(&path, ctx);

	*out = git_buf_detach(&path);

	return error;
}

static int parse_header_git_oldpath(git_patch *patch, patch_parse_ctx *ctx)
{
	return parse_header_path((char **)&patch->ofile.file->path, ctx);
}

static int parse_header_git_newpath(git_patch *patch, patch_parse_ctx *ctx)
{
	return parse_header_path((char **)&patch->nfile.file->path, ctx);
}

static int parse_header_mode(uint16_t *mode, patch_parse_ctx *ctx)
{
	const char *end;
	int32_t m;
	int ret;

	if (ctx->line_len < 1 || !git__isdigit(ctx->line[0]))
		return parse_err("invalid file mode at line %d", ctx->line_num);

	if ((ret = git__strntol32(&m, ctx->line, ctx->line_len, &end, 8)) < 0)
		return ret;

	if (m > UINT16_MAX)
		return -1;

	*mode = (uint16_t)m;

	parse_advance_chars(ctx, (end - ctx->line));

	return ret;
}

static int parse_header_oid(
	git_oid *oid,
	size_t *oid_len,
	patch_parse_ctx *ctx)
{
	size_t len;

	for (len = 0; len < ctx->line_len && len < GIT_OID_HEXSZ; len++) {
		if (!git__isxdigit(ctx->line[len]))
			break;
	}

	if (len < GIT_OID_MINPREFIXLEN ||
		git_oid_fromstrn(oid, ctx->line, len) < 0)
		return parse_err("invalid hex formatted object id at line %d",
			ctx->line_num);

	parse_advance_chars(ctx, len);

	*oid_len = len;

	return 0;
}

static int parse_header_git_index(git_patch *patch, patch_parse_ctx *ctx)
{
	/*
	 * TODO: we read the prefix provided in the diff into the delta's id
	 * field, but do not mark is at an abbreviated id.
	 */
	size_t oid_len, nid_len;

	if (parse_header_oid(&patch->delta->old_file.id, &oid_len, ctx) < 0 ||
		parse_advance_expected(ctx, "..", 2) < 0 ||
		parse_header_oid(&patch->delta->new_file.id, &nid_len, ctx) < 0)
		return -1;

	if (ctx->line_len > 0 && ctx->line[0] == ' ') {
		uint16_t mode;

		parse_advance_chars(ctx, 1);

		if (parse_header_mode(&mode, ctx) < 0)
			return -1;

		if (!patch->delta->new_file.mode)
			patch->delta->new_file.mode = mode;

		if (!patch->delta->old_file.mode)
			patch->delta->old_file.mode = mode;
	}

	return 0;
}

static int parse_header_git_oldmode(git_patch *patch, patch_parse_ctx *ctx)
{
	return parse_header_mode(&patch->ofile.file->mode, ctx);
}

static int parse_header_git_newmode(git_patch *patch, patch_parse_ctx *ctx)
{
	return parse_header_mode(&patch->nfile.file->mode, ctx);
}

static int parse_header_git_deletedfilemode(
	git_patch *patch,
	patch_parse_ctx *ctx)
{
	git__free((char *)patch->ofile.file->path);

	patch->ofile.file->path = NULL;
	patch->delta->status = GIT_DELTA_DELETED;

	return parse_header_mode(&patch->ofile.file->mode, ctx);
}

static int parse_header_git_newfilemode(
	git_patch *patch,
	patch_parse_ctx *ctx)
{
	git__free((char *)patch->nfile.file->path);

	patch->nfile.file->path = NULL;
	patch->delta->status = GIT_DELTA_ADDED;

	return parse_header_mode(&patch->nfile.file->mode, ctx);
}

static int parse_header_rename(
	char **out,
	char **header_path,
	patch_parse_ctx *ctx)
{
	git_buf path = GIT_BUF_INIT;
	size_t header_path_len, prefix_len;

	if (*header_path == NULL)
		return parse_err("rename without proper git diff header at line %d",
			ctx->line_num);

	header_path_len = strlen(*header_path);

	if (parse_header_path_buf(&path, ctx) < 0)
		return -1;

	if (header_path_len < git_buf_len(&path))
		return parse_err("rename path is invalid at line %d", ctx->line_num);

	/* This sanity check exists because git core uses the data in the
	 * "rename from" / "rename to" lines, but it's formatted differently
	 * than the other paths and lacks the normal prefix.  This irregularity
	 * causes us to ignore these paths (we always store the prefixed paths)
	 * but instead validate that they match the suffix of the paths we parsed
	 * since we would behave differently from git core if they ever differed.
	 * Instead, we raise an error, rather than parsing differently.
	 */
	prefix_len = header_path_len - path.size;

	if (strncmp(*header_path + prefix_len, path.ptr, path.size) != 0 ||
		(prefix_len > 0 && (*header_path)[prefix_len - 1] != '/'))
		return parse_err("rename path does not match header at line %d",
			ctx->line_num);

	*out = *header_path;
	*header_path = NULL;

	git_buf_free(&path);

	return 0;
}

static int parse_header_renamefrom(git_patch *patch, patch_parse_ctx *ctx)
{
	patch->delta->status |= GIT_DELTA_RENAMED;

	return parse_header_rename(
		(char **)&patch->ofile.file->path,
		&ctx->header_old_path,
		ctx);
}

static int parse_header_renameto(git_patch *patch, patch_parse_ctx *ctx)
{
	patch->delta->status |= GIT_DELTA_RENAMED;

	return parse_header_rename(
		(char **)&patch->nfile.file->path,
		&ctx->header_new_path,
		ctx);
}

static int parse_header_percent(uint16_t *out, patch_parse_ctx *ctx)
{
	int32_t val;
	const char *end;

	if (ctx->line_len < 1 || !git__isdigit(ctx->line[0]) ||
		git__strntol32(&val, ctx->line, ctx->line_len, &end, 10) < 0)
		return -1;

	parse_advance_chars(ctx, (end - ctx->line));

	if (parse_advance_expected(ctx, "%", 1) < 0)
		return -1;

	if (val > 100)
		return -1;

	*out = val;
	return 0;
}

static int parse_header_similarity(git_patch *patch, patch_parse_ctx *ctx)
{
	if (parse_header_percent(&patch->delta->similarity, ctx) < 0)
		return parse_err("invalid similarity percentage at line %d",
			ctx->line_num);

	return 0;
}

static int parse_header_dissimilarity(git_patch *patch, patch_parse_ctx *ctx)
{
	uint16_t dissimilarity;

	if (parse_header_percent(&dissimilarity, ctx) < 0)
		return parse_err("invalid similarity percentage at line %d",
			ctx->line_num);

	patch->delta->similarity = 100 - dissimilarity;

	return 0;
}

typedef struct {
	const char *str;
	int (*fn)(git_patch *, patch_parse_ctx *);
} header_git_op;

static const header_git_op header_git_ops[] = {
	{ "@@ -", NULL },
	{ "GIT binary patch", NULL },
	{ "--- ", parse_header_git_oldpath },
	{ "+++ ", parse_header_git_newpath },
	{ "index ", parse_header_git_index },
	{ "old mode ", parse_header_git_oldmode },
	{ "new mode ", parse_header_git_newmode },
	{ "deleted file mode ", parse_header_git_deletedfilemode },
	{ "new file mode ", parse_header_git_newfilemode },
	{ "rename from ", parse_header_renamefrom },
	{ "rename to ", parse_header_renameto },
	{ "rename old ", parse_header_renamefrom },
	{ "rename new ", parse_header_renameto },
	{ "similarity index ", parse_header_similarity },
	{ "dissimilarity index ", parse_header_dissimilarity },
};

static int parse_header_git(
	git_patch *patch,
	patch_parse_ctx *ctx)
{
	size_t i;
	int error = 0;

	/* Parse the diff --git line */
	if (parse_advance_expected(ctx, "diff --git ", 11) < 0)
		return parse_err("corrupt git diff header at line %d", ctx->line_num);

	if (parse_header_path(&ctx->header_old_path, ctx) < 0)
		return parse_err("corrupt old path in git diff header at line %d",
			ctx->line_num);

	if (parse_advance_ws(ctx) < 0 ||
		parse_header_path(&ctx->header_new_path, ctx) < 0)
		return parse_err("corrupt new path in git diff header at line %d",
			ctx->line_num);

	/* Parse remaining header lines */
	for (parse_advance_line(ctx); ctx->remain > 0; parse_advance_line(ctx)) {
		if (ctx->line_len == 0 || ctx->line[ctx->line_len - 1] != '\n')
			break;

		for (i = 0; i < ARRAY_SIZE(header_git_ops); i++) {
			const header_git_op *op = &header_git_ops[i];
			size_t op_len = strlen(op->str);

			if (memcmp(ctx->line, op->str, min(op_len, ctx->line_len)) != 0)
				continue;

			/* Do not advance if this is the patch separator */
			if (op->fn == NULL)
				goto done;

			parse_advance_chars(ctx, op_len);

			if ((error = op->fn(patch, ctx)) < 0)
				goto done;

			parse_advance_ws(ctx);
			parse_advance_expected(ctx, "\n", 1);

			if (ctx->line_len > 0) {
				error = parse_err("trailing data at line %d", ctx->line_num);
				goto done;
			}

			break;
		}
	}

done:
	return error;
}

static int parse_number(git_off_t *out, patch_parse_ctx *ctx)
{
	const char *end;
	int64_t num;

	if (!git__isdigit(ctx->line[0]))
		return -1;

	if (git__strntol64(&num, ctx->line, ctx->line_len, &end, 10) < 0)
		return -1;

	if (num < 0)
		return -1;

	*out = num;
	parse_advance_chars(ctx, (end - ctx->line));

	return 0;
}

static int parse_int(int *out, patch_parse_ctx *ctx)
{
	git_off_t num;

	if (parse_number(&num, ctx) < 0 || !git__is_int(num))
		return -1;

	*out = (int)num;
	return 0;
}

static int parse_hunk_header(
	diff_patch_hunk *hunk,
	patch_parse_ctx *ctx)
{
	const char *header_start = ctx->line;

	hunk->hunk.old_lines = 1;
	hunk->hunk.new_lines = 1;

	if (parse_advance_expected(ctx, "@@ -", 4) < 0 ||
		parse_int(&hunk->hunk.old_start, ctx) < 0)
		goto fail;

	if (ctx->line_len > 0 && ctx->line[0] == ',') {
		if (parse_advance_expected(ctx, ",", 1) < 0 ||
			parse_int(&hunk->hunk.old_lines, ctx) < 0)
			goto fail;
	}

	if (parse_advance_expected(ctx, " +", 2) < 0 ||
		parse_int(&hunk->hunk.new_start, ctx) < 0)
		goto fail;

	if (ctx->line_len > 0 && ctx->line[0] == ',') {
		if (parse_advance_expected(ctx, ",", 1) < 0 ||
			parse_int(&hunk->hunk.new_lines, ctx) < 0)
			goto fail;
	}

	if (parse_advance_expected(ctx, " @@", 3) < 0)
		goto fail;

	parse_advance_line(ctx);

	if (!hunk->hunk.old_lines && !hunk->hunk.new_lines)
		goto fail;

	hunk->hunk.header_len = ctx->line - header_start;
	if (hunk->hunk.header_len > (GIT_DIFF_HUNK_HEADER_SIZE - 1))
		return parse_err("oversized patch hunk header at line %d",
			ctx->line_num);

	memcpy(hunk->hunk.header, header_start, hunk->hunk.header_len);
	hunk->hunk.header[hunk->hunk.header_len] = '\0';

	return 0;

fail:
	giterr_set(GITERR_PATCH, "invalid patch hunk header at line %d",
		ctx->line_num);
	return -1;
}

static int parse_hunk_body(
	git_patch *patch,
	diff_patch_hunk *hunk,
	patch_parse_ctx *ctx)
{
	git_diff_line *line;
	int error = 0;

	int oldlines = hunk->hunk.old_lines;
	int newlines = hunk->hunk.new_lines;

	for (;
		ctx->remain > 4 && (oldlines || newlines) &&
		memcmp(ctx->line, "@@ -", 4) != 0;
		parse_advance_line(ctx)) {

		int origin;
		int prefix = 1;

		if (ctx->line_len == 0 || ctx->line[ctx->line_len - 1] != '\n') {
			error = parse_err("invalid patch instruction at line %d",
				ctx->line_num);
			goto done;
		}

		switch (ctx->line[0]) {
		case '\n':
			prefix = 0;

		case ' ':
			origin = GIT_DIFF_LINE_CONTEXT;
			oldlines--;
			newlines--;
			break;

		case '-':
			origin = GIT_DIFF_LINE_DELETION;
			oldlines--;
			break;

		case '+':
			origin = GIT_DIFF_LINE_ADDITION;
			newlines--;
			break;

		default:
			error = parse_err("invalid patch hunk at line %d", ctx->line_num);
			goto done;
		}

		line = git_array_alloc(patch->lines);
		GITERR_CHECK_ALLOC(line);

		memset(line, 0x0, sizeof(git_diff_line));

		line->content = ctx->line + prefix;
		line->content_len = ctx->line_len - prefix;
		line->content_offset = ctx->content_len - ctx->remain;
		line->origin = origin;

		hunk->line_count++;
	}

	if (oldlines || newlines) {
		error = parse_err(
			"invalid patch hunk, expected %d old lines and %d new lines",
			hunk->hunk.old_lines, hunk->hunk.new_lines);
		goto done;
	}

	/* Handle "\ No newline at end of file".  Only expect the leading
	 * backslash, though, because the rest of the string could be
	 * localized.  Because `diff` optimizes for the case where you
	 * want to apply the patch by hand.
	 */
	if (ctx->line_len >= 2 && memcmp(ctx->line, "\\ ", 2) == 0 &&
		git_array_size(patch->lines) > 0) {

		line = git_array_get(patch->lines, git_array_size(patch->lines)-1);

		if (line->content_len < 1) {
			error = parse_err("cannot trim trailing newline of empty line");
			goto done;
		}

		line->content_len--;

		parse_advance_line(ctx);
	}

done:
	return error;
}

static int parse_header_traditional(git_patch *patch, patch_parse_ctx *ctx)
{
	GIT_UNUSED(patch);
	GIT_UNUSED(ctx);

	return 1;
}

static int parse_patch_header(
	git_patch *patch,
	patch_parse_ctx *ctx)
{
	int error = 0;

	for (ctx->line = ctx->content; ctx->remain > 0; parse_advance_line(ctx)) {
		/* This line is too short to be a patch header. */
		if (ctx->line_len < 6)
			continue;

		/* This might be a hunk header without a patch header, provide a
		 * sensible error message. */
		if (memcmp(ctx->line, "@@ -", 4) == 0) {
			size_t line_num = ctx->line_num;
			diff_patch_hunk hunk;

			/* If this cannot be parsed as a hunk header, it's just leading
			 * noise, continue.
			 */
			if (parse_hunk_header(&hunk, ctx) < 0) {
				giterr_clear();
				continue;
			}

			error = parse_err("invalid hunk header outside patch at line %d",
				line_num);
			goto done;
		}

		/* This buffer is too short to contain a patch. */
		if (ctx->remain < ctx->line_len + 6)
			break;

		/* A proper git patch */
		if (ctx->line_len >= 11 && memcmp(ctx->line, "diff --git ", 11) == 0) {
			if ((error = parse_header_git(patch, ctx)) < 0)
				goto done;

			/* For modechange only patches, it does not include filenames;
			 * instead we need to use the paths in the diff --git header.
			 */
			if (!patch->ofile.file->path && !patch->nfile.file->path) {
				if (!ctx->header_old_path || !ctx->header_new_path) {
					error = parse_err("git diff header lacks old / new paths");
					goto done;
				}

				patch->ofile.file->path = ctx->header_old_path;
				ctx->header_old_path = NULL;

				patch->nfile.file->path = ctx->header_new_path;
				ctx->header_new_path = NULL;
			}

			goto done;
		}

		if ((error = parse_header_traditional(patch, ctx)) <= 0)
			goto done;

		error = 0;
		continue;
	}

	error = parse_err("no header in patch file");

done:
	return error;
}

static int parse_patch_binary_side(
	git_diff_binary_file *binary,
	patch_parse_ctx *ctx)
{
	git_diff_binary_t type = GIT_DIFF_BINARY_NONE;
	git_buf base85 = GIT_BUF_INIT, decoded = GIT_BUF_INIT;
	git_off_t len;
	int error = 0;

	if (ctx->line_len >= 8 && memcmp(ctx->line, "literal ", 8) == 0) {
		type = GIT_DIFF_BINARY_LITERAL;
		parse_advance_chars(ctx, 8);
	} else if (ctx->line_len >= 6 && memcmp(ctx->line, "delta ", 6) == 0) {
		type = GIT_DIFF_BINARY_DELTA;
		parse_advance_chars(ctx, 6);
	} else {
		error = parse_err("unknown binary delta type at line %d", ctx->line_num);
		goto done;
	}

	if (parse_number(&len, ctx) < 0 || parse_advance_nl(ctx) < 0 || len < 0) {
		error = parse_err("invalid binary size at line %d", ctx->line_num);
		goto done;
	}

	while (ctx->line_len) {
		char c = ctx->line[0];
		size_t encoded_len, decoded_len = 0, decoded_orig = decoded.size;

		if (c == '\n')
			break;
		else if (c >= 'A' && c <= 'Z')
			decoded_len = c - 'A' + 1;
		else if (c >= 'a' && c <= 'z')
			decoded_len = c - 'a' + 26 + 1;

		if (!decoded_len) {
			error = parse_err("invalid binary length at line %d", ctx->line_num);
			goto done;
		}

		parse_advance_chars(ctx, 1);

		encoded_len = ((decoded_len / 4) + !!(decoded_len % 4)) * 5;

		if (encoded_len > ctx->line_len - 1) {
			error = parse_err("truncated binary data at line %d", ctx->line_num);
			goto done;
		}

		if ((error = git_buf_decode_base85(
				&decoded, ctx->line, encoded_len, decoded_len)) < 0)
			goto done;

		if (decoded.size - decoded_orig != decoded_len) {
			error = parse_err("truncated binary data at line %d", ctx->line_num);
			goto done;
		}

		parse_advance_chars(ctx, encoded_len);

		if (parse_advance_nl(ctx) < 0) {
			error = parse_err("trailing data at line %d", ctx->line_num);
			goto done;
		}
	}

	binary->type = type;
	binary->inflatedlen = (size_t)len;
	binary->datalen = decoded.size;
	binary->data = git_buf_detach(&decoded);

done:
	git_buf_free(&base85);
	git_buf_free(&decoded);
	return error;
}

static int parse_patch_binary(
	git_patch *patch,
	patch_parse_ctx *ctx)
{
	int error;

	if (parse_advance_expected(ctx, "GIT binary patch", 16) < 0 ||
		parse_advance_nl(ctx) < 0)
		return parse_err("corrupt git binary header at line %d", ctx->line_num);

	/* parse old->new binary diff */
	if ((error = parse_patch_binary_side(&patch->binary.new_file, ctx)) < 0)
		return error;

	if (parse_advance_nl(ctx) < 0)
		return parse_err("corrupt git binary separator at line %d", ctx->line_num);

	/* parse new->old binary diff */
	if ((error = parse_patch_binary_side(&patch->binary.old_file, ctx)) < 0)
		return error;

	patch->delta->flags |= GIT_DIFF_FLAG_BINARY;
	return 0;
}

static int parse_patch_hunks(
	git_patch *patch,
	patch_parse_ctx *ctx)
{
	diff_patch_hunk *hunk;
	int error = 0;

	for (; ctx->line_len > 4 && memcmp(ctx->line, "@@ -", 4) == 0; ) {

		hunk = git_array_alloc(patch->hunks);
		GITERR_CHECK_ALLOC(hunk);

		memset(hunk, 0, sizeof(diff_patch_hunk));

		hunk->line_start = git_array_size(patch->lines);
		hunk->line_count = 0;

		if ((error = parse_hunk_header(hunk, ctx)) < 0 ||
			(error = parse_hunk_body(patch, hunk, ctx)) < 0)
			goto done;
	}

done:
	return error;
}

static int parse_patch_body(git_patch *patch, patch_parse_ctx *ctx)
{
	if (ctx->line_len >= 16 && memcmp(ctx->line, "GIT binary patch", 16) == 0)
		return parse_patch_binary(patch, ctx);

	else if (ctx->line_len >= 4 && memcmp(ctx->line, "@@ -", 4) == 0)
		return parse_patch_hunks(patch, ctx);

	return 0;
}

static int check_patch(git_patch *patch)
{
	if (!patch->ofile.file->path && patch->delta->status != GIT_DELTA_ADDED)
		return parse_err("missing old file path");

	if (!patch->nfile.file->path && patch->delta->status != GIT_DELTA_DELETED)
		return parse_err("missing new file path");

	if (patch->ofile.file->path && patch->nfile.file->path) {
		if (!patch->nfile.file->mode)
			patch->nfile.file->mode = patch->ofile.file->mode;
	}

	if (patch->delta->status == GIT_DELTA_MODIFIED &&
			!(patch->delta->flags & GIT_DIFF_FLAG_BINARY) &&
			patch->nfile.file->mode == patch->ofile.file->mode &&
			git_array_size(patch->hunks) == 0)
		return parse_err("patch with no hunks");

	return 0;
}

int git_patch_from_patchfile(
	git_patch **out,
	const char *content,
	size_t content_len)
{
	patch_parse_ctx ctx = {0};
	git_patch *patch;
	int error = 0;

	*out = NULL;

	patch = git__calloc(1, sizeof(git_patch));
	GITERR_CHECK_ALLOC(patch);

	patch->delta = git__calloc(1, sizeof(git_diff_delta));
	patch->ofile.file = git__calloc(1, sizeof(git_diff_file));
	patch->nfile.file = git__calloc(1, sizeof(git_diff_file));

	patch->delta->status = GIT_DELTA_MODIFIED;

	ctx.content = content;
	ctx.content_len = content_len;
	ctx.remain = content_len;

	if ((error = parse_patch_header(patch, &ctx)) < 0 ||
		(error = parse_patch_body(patch, &ctx)) < 0 ||
		(error = check_patch(patch)) < 0)
		goto done;

	*out = patch;

done:
	git__free(ctx.header_old_path);
	git__free(ctx.header_new_path);

	return error;
}
