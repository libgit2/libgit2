/*
 * Copyright (C) 2009-2011 the libgit2 contributors
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "common.h"
#include "signature.h"
#include "repository.h"
#include "git2/common.h"

void git_signature_free(git_signature *sig)
{
	if (sig == NULL)
		return;

	git__free(sig->name);
	git__free(sig->email);
	git__free(sig);
}

static const char *skip_leading_spaces(const char *buffer, const char *buffer_end)
{
	while (*buffer == ' ' && buffer < buffer_end)
		buffer++;

	return buffer;
}

static const char *skip_trailing_spaces(const char *buffer_start, const char *buffer_end)
{
	while (*buffer_end == ' ' && buffer_end > buffer_start)
		buffer_end--;

	return buffer_end;
}

static int process_trimming(const char *input, char **storage, const char *input_end, int fail_when_empty)
{
	const char *left, *right;
	int trimmed_input_length;

	left = skip_leading_spaces(input, input_end);
	right = skip_trailing_spaces(input, input_end - 1);

	if (right < left) {
		if (fail_when_empty)
			return git__throw(GIT_EINVALIDARGS, "Failed to trim. Input is either empty or only contains spaces");
		else
			right = left - 1;
	}

	trimmed_input_length = right - left + 1;

	*storage = git__malloc(trimmed_input_length + 1);
	if (*storage == NULL)
		return GIT_ENOMEM;

	memcpy(*storage, left, trimmed_input_length);
	(*storage)[trimmed_input_length] = 0;

	return GIT_SUCCESS;
}

int git_signature_new(git_signature **sig_out, const char *name, const char *email, git_time_t time, int offset)
{
	int error;
	git_signature *p = NULL;

	assert(name && email);

	*sig_out = NULL;

	if ((p = git__malloc(sizeof(git_signature))) == NULL) {
		error = GIT_ENOMEM;
		goto cleanup;
	}

	memset(p, 0x0, sizeof(git_signature));

	error = process_trimming(name, &p->name, name + strlen(name), 1);
	if (error < GIT_SUCCESS) {
		git__rethrow(GIT_EINVALIDARGS, "Failed to create signature. 'name' argument is invalid");
		goto cleanup;
	}

	error = process_trimming(email, &p->email, email + strlen(email), 1);
	if (error < GIT_SUCCESS) {
		git__rethrow(GIT_EINVALIDARGS, "Failed to create signature. 'email' argument is invalid");
		goto cleanup;
	}

	p->when.time = time;
	p->when.offset = offset;

	*sig_out = p;

	return error;

cleanup:
	git_signature_free(p);
	return error;
}

git_signature *git_signature_dup(const git_signature *sig)
{
	git_signature *new;
	if (git_signature_new(&new, sig->name, sig->email, sig->when.time, sig->when.offset) < GIT_SUCCESS)
		return NULL;
	return new;
}

int git_signature_now(git_signature **sig_out, const char *name, const char *email)
{
	int error;
	time_t now;
	time_t offset;
	struct tm *utc_tm, *local_tm;
	git_signature *sig;

#ifndef GIT_WIN32
	struct tm _utc, _local;
#endif

	*sig_out = NULL;

	time(&now);

	/**
	 * On Win32, `gmtime_r` doesn't exist but
	 * `gmtime` is threadsafe, so we can use that
	 */
#ifdef GIT_WIN32
	utc_tm = gmtime(&now);
	local_tm = localtime(&now);
#else
	utc_tm = gmtime_r(&now, &_utc);
	local_tm = localtime_r(&now, &_local);
#endif

	offset = mktime(local_tm) - mktime(utc_tm);
	offset /= 60;

	/* mktime takes care of setting tm_isdst correctly */
	if (local_tm->tm_isdst)
		offset += 60;

	if ((error = git_signature_new(&sig, name, email, now, (int)offset)) < GIT_SUCCESS)
		return error;

	*sig_out = sig;

	return error;
}

static int parse_timezone_offset(const char *buffer, int *offset_out)
{
	int dec_offset;
	int mins, hours, offset;

	const char *offset_start;
	const char *offset_end;

	offset_start = buffer;

	if (*offset_start == '\n') {
		*offset_out = 0;
		return GIT_SUCCESS;
	}

	if (offset_start[0] != '-' && offset_start[0] != '+')
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse TZ offset. It doesn't start with '+' or '-'");

	if (offset_start[1] < '0' || offset_start[1] > '9')
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse TZ offset.");

	if (git__strtol32(&dec_offset, offset_start + 1, &offset_end, 10) < GIT_SUCCESS)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse TZ offset. It isn't a number");

	if (offset_end - offset_start != 5)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse TZ offset. Invalid length");

	if (dec_offset > 1400)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse TZ offset. Value too large");

	hours = dec_offset / 100;
	mins = dec_offset % 100;

	if (hours > 14)	// see http://www.worldtimezone.com/faq.html
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse TZ offset. Hour value too large");

	if (mins > 59)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse TZ offset. Minute value too large");

	offset = (hours * 60) + mins;

	if (offset_start[0] == '-')
		offset *= -1;

	*offset_out = offset;

	return GIT_SUCCESS;
}

static int process_next_token(const char **buffer_out, char **storage,
	const char *token_end, const char *right_boundary)
{
	int error = process_trimming(*buffer_out, storage, token_end, 0);
	if (error < GIT_SUCCESS)
		return error;

	*buffer_out = token_end + 1;

	if (*buffer_out > right_boundary)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse signature. Signature too short");

	return GIT_SUCCESS;
}

static const char *scan_for_previous_token(const char *buffer, const char *left_boundary)
{
	const char *start;

	if (buffer <= left_boundary)
		return NULL;

	start = skip_trailing_spaces(left_boundary, buffer);

	/* Search for previous occurence of space */
	while (start[-1] != ' ' && start > left_boundary)
		start--;

	return start;
}

static int parse_time(git_time_t *time_out, const char *buffer)
{
	int time;
	int error;

	if (*buffer == '+' || *buffer == '-')
		return git__throw(GIT_ERROR, "Failed while parsing time. '%s' rather look like a timezone offset.", buffer);

	error = git__strtol32(&time, buffer, &buffer, 10);

	if (error < GIT_SUCCESS)
		return error;

	*time_out = (git_time_t)time;

	return GIT_SUCCESS;
}

int git_signature__parse(git_signature *sig, const char **buffer_out,
		const char *buffer_end, const char *header, char ender)
{
	const char *buffer = *buffer_out;
	const char *line_end, *name_end, *email_end, *tz_start, *time_start;
	int error = GIT_SUCCESS;

	memset(sig, 0x0, sizeof(git_signature));

	if ((line_end = memchr(buffer, ender, buffer_end - buffer)) == NULL)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse signature. No newline given");

	if (header) {
		const size_t header_len = strlen(header);

		if (memcmp(buffer, header, header_len) != 0)
			return git__throw(GIT_EOBJCORRUPTED, "Failed to parse signature. Expected prefix '%s' doesn't match actual", header);

		buffer += header_len;
	}

	if (buffer > line_end)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse signature. Signature too short");

	if ((name_end = strchr(buffer, '<')) == NULL)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse signature. Cannot find '<' in signature");

	if ((email_end = strchr(name_end, '>')) == NULL)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse signature. Cannot find '>' in signature");

	if (email_end < name_end)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse signature. Malformed e-mail");

	error = process_next_token(&buffer, &sig->name, name_end, line_end);
	if (error < GIT_SUCCESS)
		return error;

	error = process_next_token(&buffer, &sig->email, email_end, line_end);
	if (error < GIT_SUCCESS)
		return error;

	tz_start = scan_for_previous_token(line_end - 1, buffer);

	if (tz_start == NULL)
		goto clean_exit;	/* No timezone nor date */

	time_start = scan_for_previous_token(tz_start - 1, buffer);
	if (time_start == NULL || parse_time(&sig->when.time, time_start) < GIT_SUCCESS) {
		/* The tz_start might point at the time */
		parse_time(&sig->when.time, tz_start);
		goto clean_exit;
	}

	if (parse_timezone_offset(tz_start, &sig->when.offset) < GIT_SUCCESS) {
		sig->when.time = 0; /* Bogus timezone, we reset the time */
	}

clean_exit:
	*buffer_out = line_end + 1;
	return GIT_SUCCESS;
}

void git_signature__writebuf(git_buf *buf, const char *header, const git_signature *sig)
{
	int offset, hours, mins;
	char sign;

	offset = sig->when.offset;
	sign = (sig->when.offset < 0) ? '-' : '+';

	if (offset < 0)
		offset = -offset;

	hours = offset / 60;
	mins = offset % 60;

	git_buf_printf(buf, "%s%s <%s> %u %c%02d%02d\n",
			header ? header : "", sig->name, sig->email,
			(unsigned)sig->when.time, sign, hours, mins);
}

