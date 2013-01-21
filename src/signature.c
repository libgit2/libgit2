/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
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
	sig->name = NULL;
	git__free(sig->email);
	sig->email = NULL;
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

static int signature_error(const char *msg)
{
	giterr_set(GITERR_INVALID, "Failed to process signature - %s", msg);
	return -1;
}

static int process_trimming(const char *input, char **storage, const char *input_end, int fail_when_empty)
{
	const char *left, *right;
	size_t trimmed_input_length;

	assert(storage);

	left = skip_leading_spaces(input, input_end);
	right = skip_trailing_spaces(input, input_end - 1);

	if (right < left) {
		if (fail_when_empty)
			return signature_error("input is either empty of contains only spaces");

		right = left - 1;
	}

	trimmed_input_length = right - left + 1;

	*storage = git__malloc(trimmed_input_length + 1);
	GITERR_CHECK_ALLOC(*storage);

	memcpy(*storage, left, trimmed_input_length);
	(*storage)[trimmed_input_length] = 0;

	return 0;
}

static bool contains_angle_brackets(const char *input)
{
	if (strchr(input, '<') != NULL)
		return true;

	return strchr(input, '>') != NULL;
}

int git_signature_new(git_signature **sig_out, const char *name, const char *email, git_time_t time, int offset)
{
	git_signature *p = NULL;

	assert(name && email);

	*sig_out = NULL;

	p = git__calloc(1, sizeof(git_signature));
	GITERR_CHECK_ALLOC(p);

	if (process_trimming(name, &p->name, name + strlen(name), 1) < 0 ||
		process_trimming(email, &p->email, email + strlen(email), 1) < 0)
	{
		git_signature_free(p);
		return -1;
	}
		
	if (contains_angle_brackets(p->email) ||
		contains_angle_brackets(p->name))
	{
		git_signature_free(p);
		return signature_error("Neither `name` nor `email` should contain angle brackets chars.");
	}

	p->when.time = time;
	p->when.offset = offset;

	*sig_out = p;

	return 0;
}

git_signature *git_signature_dup(const git_signature *sig)
{
	git_signature *new;
	if (git_signature_new(&new, sig->name, sig->email, sig->when.time, sig->when.offset) < 0)
		return NULL;
	return new;
}

int git_signature_now(git_signature **sig_out, const char *name, const char *email)
{
	time_t now;
	time_t offset;
	struct tm *utc_tm;
	git_signature *sig;
	struct tm _utc;

	*sig_out = NULL;

	/*
	 * Get the current time as seconds since the epoch and
	 * transform that into a tm struct containing the time at
	 * UTC. Give that to mktime which considers it a local time
	 * (tm_isdst = -1 asks it to take DST into account) and gives
	 * us that time as seconds since the epoch. The difference
	 * between its return value and 'now' is our offset to UTC.
	 */
	time(&now);
	utc_tm = p_gmtime_r(&now, &_utc);
	utc_tm->tm_isdst = -1;
	offset = (time_t)difftime(now, mktime(utc_tm));
	offset /= 60;

	if (git_signature_new(&sig, name, email, now, (int)offset) < 0)
		return -1;

	*sig_out = sig;

	return 0;
}

static int timezone_error(const char *msg)
{
	giterr_set(GITERR_INVALID, "Failed to parse TZ offset - %s", msg);
	return -1;
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
		return 0;
	}

	if (offset_start[0] != '-' && offset_start[0] != '+')
		return timezone_error("does not start with '+' or '-'");

	if (offset_start[1] < '0' || offset_start[1] > '9')
		return timezone_error("expected initial digit");

	if (git__strtol32(&dec_offset, offset_start + 1, &offset_end, 10) < 0)
		return timezone_error("not a valid number");

	if (offset_end - offset_start != 5)
		return timezone_error("invalid length");

	if (dec_offset > 1400)
		return timezone_error("value too large");

	hours = dec_offset / 100;
	mins = dec_offset % 100;

	if (hours > 14)	// see http://www.worldtimezone.com/faq.html
		return timezone_error("hour value too large");

	if (mins > 59)
		return timezone_error("minutes value too large");

	offset = (hours * 60) + mins;

	if (offset_start[0] == '-')
		offset *= -1;

	*offset_out = offset;

	return 0;
}

static int process_next_token(const char **buffer_out, char **storage,
	const char *token_end, const char *right_boundary)
{
	int error = process_trimming(*buffer_out, storage, token_end, 0);
	if (error < 0)
		return error;

	*buffer_out = token_end + 1;

	if (*buffer_out > right_boundary)
		return signature_error("signature is too short");

	return 0;
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
	int error;
	int64_t time;

	if (*buffer == '+' || *buffer == '-') {
		giterr_set(GITERR_INVALID, "Failed while parsing time. '%s' actually looks like a timezone offset.", buffer);
		return -1;
	}

	error = git__strtol64(&time, buffer, &buffer, 10);

	if (!error)
		*time_out = (git_time_t)time;

	return error;
}

int git_signature__parse(git_signature *sig, const char **buffer_out,
		const char *buffer_end, const char *header, char ender)
{
	const char *buffer = *buffer_out;
	const char *line_end, *name_end, *email_end, *tz_start, *time_start;
	int error = 0;

	memset(sig, 0, sizeof(git_signature));

	if ((line_end = memchr(buffer, ender, buffer_end - buffer)) == NULL)
		return signature_error("no newline given");

	if (header) {
		const size_t header_len = strlen(header);

		if (memcmp(buffer, header, header_len) != 0)
			return signature_error("expected prefix doesn't match actual");

		buffer += header_len;
	}

	if (buffer > line_end)
		return signature_error("signature too short");

	if ((name_end = strchr(buffer, '<')) == NULL)
		return signature_error("character '<' not allowed in signature");

	if ((email_end = strchr(name_end, '>')) == NULL)
		return signature_error("character '>' not allowed in signature");

	if (email_end < name_end)
		return signature_error("malformed e-mail");

	error = process_next_token(&buffer, &sig->name, name_end, line_end);
	if (error < 0)
		return error;

	error = process_next_token(&buffer, &sig->email, email_end, line_end);
	if (error < 0)
		return error;

	tz_start = scan_for_previous_token(line_end - 1, buffer);

	if (tz_start == NULL)
		goto clean_exit;	/* No timezone nor date */

	time_start = scan_for_previous_token(tz_start - 1, buffer);
	if (time_start == NULL || parse_time(&sig->when.time, time_start) < 0) {
		/* The tz_start might point at the time */
		parse_time(&sig->when.time, tz_start);
		goto clean_exit;
	}

	if (parse_timezone_offset(tz_start, &sig->when.offset) < 0) {
		sig->when.time = 0; /* Bogus timezone, we reset the time */
	}

clean_exit:
	*buffer_out = line_end + 1;
	return 0;
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

