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

static int signature_error(const char *msg)
{
	giterr_set(GITERR_INVALID, "Failed to parse signature - %s", msg);
	return -1;
}

static bool contains_angle_brackets(const char *input)
{
	return strchr(input, '<') != NULL || strchr(input, '>') != NULL;
}

static char *extract_trimmed(const char *ptr, size_t len)
{
	while (len && ptr[0] == ' ') {
		ptr++; len--;
	}

	while (len && ptr[len - 1] == ' ') {
		len--;
	}

	return git__substrdup(ptr, len);
}

int git_signature_new(git_signature **sig_out, const char *name, const char *email, git_time_t time, int offset)
{
	git_signature *p = NULL;

	assert(name && email);

	*sig_out = NULL;

	if (contains_angle_brackets(name) ||
		contains_angle_brackets(email)) {
		return signature_error(
			"Neither `name` nor `email` should contain angle brackets chars.");
	}

	p = git__calloc(1, sizeof(git_signature));
	GITERR_CHECK_ALLOC(p);

	p->name = extract_trimmed(name, strlen(name));
	p->email = extract_trimmed(email, strlen(email));

	if (p->name == NULL || p->email == NULL ||
		p->name[0] == '\0' || p->email[0] == '\0') {
		git_signature_free(p);
		return -1;
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

int git_signature__parse(git_signature *sig, const char **buffer_out,
		const char *buffer_end, const char *header, char ender)
{
	const char *buffer = *buffer_out;
	const char *email_start, *email_end;

	memset(sig, 0, sizeof(git_signature));

	if ((buffer_end = memchr(buffer, ender, buffer_end - buffer)) == NULL)
		return signature_error("no newline given");

	if (header) {
		const size_t header_len = strlen(header);

		if (buffer + header_len >= buffer_end || memcmp(buffer, header, header_len) != 0)
			return signature_error("expected prefix doesn't match actual");

		buffer += header_len;
	}

	email_start = git__memrchr(buffer, '<', buffer_end - buffer);
	email_end = git__memrchr(buffer, '>', buffer_end - buffer);

	if (!email_start || !email_end || email_end <= email_start)
		return signature_error("malformed e-mail");

	email_start += 1;
	sig->name = extract_trimmed(buffer, email_start - buffer - 1);
	sig->email = extract_trimmed(email_start, email_end - email_start);

	/* Do we even have a time at the end of the signature? */
	if (email_end + 2 < buffer_end) {
		const char *time_start = email_end + 2;
		const char *time_end;

		if (git__strtol64(&sig->when.time, time_start, &time_end, 10) < 0)
			return signature_error("invalid Unix timestamp");

		/* do we have a timezone? */
		if (time_end + 1 < buffer_end) {
			int offset, hours, mins;
			const char *tz_start, *tz_end;

			tz_start = time_end + 1;

			if ((tz_start[0] != '-' && tz_start[0] != '+') || 
				git__strtol32(&offset, tz_start + 1, &tz_end, 10) < 0)
				return signature_error("malformed timezone");

			hours = offset / 100;
			mins = offset % 100;

			/*
			 * only store timezone if it's not overflowing;
			 * see http://www.worldtimezone.com/faq.html
			 */
			if (hours < 14 && mins < 59) {
				sig->when.offset = (hours * 60) + mins;
				if (tz_start[0] == '-')
					sig->when.offset = -sig->when.offset;
			}
		}
	}

	*buffer_out = buffer_end + 1;
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

