/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "common.h"
#include "signature.h"
#include "repository.h"
#include "git2/common.h"

void git_signature_free(git_signature *sig)
{
	if (sig == NULL)
		return;

	free(sig->name);
	free(sig->email);
	free(sig);
}

git_signature *git_signature_new(const char *name, const char *email, git_time_t time, int offset)
{
	git_signature *p = NULL;

	if ((p = git__malloc(sizeof(git_signature))) == NULL)
		goto cleanup;

	p->name = git__strdup(name);
	p->email = git__strdup(email);
	p->when.time = time;
	p->when.offset = offset;

	if (p->name == NULL || p->email == NULL)
		goto cleanup;

	return p;

cleanup:
	git_signature_free(p);
	return NULL;
}

git_signature *git_signature_dup(const git_signature *sig)
{
	return git_signature_new(sig->name, sig->email, sig->when.time, sig->when.offset);
}

git_signature *git_signature_now(const char *name, const char *email)
{
	time_t now;
	time_t offset;
	struct tm *utc_tm, *local_tm;

#ifndef GIT_WIN32
	struct tm _utc, _local;
#endif

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

	return git_signature_new(name, email, now, (int)offset);
}

static int parse_timezone_offset(const char *buffer, int *offset_out)
{
	long dec_offset;
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

int process_next_token(const char **buffer_out, char **storage,
	const char *token_end, const char *line_end)
{
	int name_length = 0;
	const char *buffer = *buffer_out;

	/* Skip leading spaces before the name */
	while (*buffer == ' ' && buffer < line_end)
		buffer++;

	name_length = token_end - buffer;

	/* Trim trailing spaces after the name */
	while (buffer[name_length - 1] == ' ' && name_length > 0)
		name_length--;

	*storage = git__malloc(name_length + 1);
	if (*storage == NULL)
		return GIT_ENOMEM;

	memcpy(*storage, buffer, name_length);
	(*storage)[name_length] = 0;
	buffer = token_end + 1;

	if (buffer > line_end)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to parse signature. Signature too short");

	*buffer_out = buffer;
	return GIT_SUCCESS;
}

const char* scan_for_previous_token(const char *buffer, const char *left_boundary)
{
	const char *start = buffer;

	if (start <= left_boundary)
		return NULL;

	/* Trim potential trailing spaces */
	while (*start == ' ' && start > left_boundary)
		start--;

	/* Search for previous occurence of space */
	while (start[-1] != ' ' && start > left_boundary)
		start--;

	return start;
}

int parse_time(git_time_t *time_out, const char *buffer)
{
	long time;
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
		const char *buffer_end, const char *header)
{
	const char *buffer = *buffer_out;
	const char *line_end, *name_end, *email_end, *tz_start, *time_start;
	int error = GIT_SUCCESS;

	memset(sig, 0x0, sizeof(git_signature));

	if ((line_end = memchr(buffer, '\n', buffer_end - buffer)) == NULL)
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

	if ((email_end = strchr(buffer, '>')) == NULL)
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

int git_signature__write(char **signature, const char *header, const git_signature *sig)
{
	int offset, hours, mins;
	char sig_buffer[2048];
	int sig_buffer_len;
	char sign;

	offset = sig->when.offset;
	sign = (sig->when.offset < 0) ? '-' : '+';

	if (offset < 0)
		offset = -offset;

	hours = offset / 60;
	mins = offset % 60;

	sig_buffer_len = snprintf(sig_buffer, sizeof(sig_buffer),
			"%s %s <%s> %u %c%02d%02d\n",
			header, sig->name, sig->email,
			(unsigned)sig->when.time, sign, hours, mins);

	if (sig_buffer_len < 0 || (size_t)sig_buffer_len > sizeof(sig_buffer))
		return GIT_ENOMEM;

	*signature = git__strdup(sig_buffer);
	return sig_buffer_len;
}


