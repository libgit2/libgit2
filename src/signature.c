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

static int parse_timezone_offset(const char *buffer, long *offset_out)
{
	long offset, dec_offset;
	int mins, hours;

	const char *offset_start;
	const char *offset_end;

	offset_start = buffer + 1;

	if (*offset_start == '\n') {
		*offset_out = 0;
		return GIT_SUCCESS;
	}

	if (offset_start[0] != '-' && offset_start[0] != '+')
		return GIT_EOBJCORRUPTED;

	if (git__strtol32(&dec_offset, offset_start + 1, &offset_end, 10) < GIT_SUCCESS)
		return GIT_EOBJCORRUPTED;

	if (offset_end - offset_start != 5)
		return GIT_EOBJCORRUPTED;

	hours = dec_offset / 100;
	mins = dec_offset % 100;

	if (hours > 14)	// see http://www.worldtimezone.com/faq.html 
		return GIT_EOBJCORRUPTED;

	if (mins > 59)
		return GIT_EOBJCORRUPTED;

	offset = (hours * 60) + mins;

	if (offset_start[0] == '-')
		offset *= -1;
	
	*offset_out = offset;

	return GIT_SUCCESS;
}


int git_signature__parse(git_signature *sig, const char **buffer_out,
		const char *buffer_end, const char *header)
{
	const size_t header_len = strlen(header);

	int name_length, email_length;
	const char *buffer = *buffer_out;
	const char *line_end, *name_end, *email_end;
	long offset = 0, time;

	memset(sig, 0x0, sizeof(git_signature));

	line_end = memchr(buffer, '\n', buffer_end - buffer);
	if (!line_end)
		return GIT_EOBJCORRUPTED;

	if (buffer + (header_len + 1) > line_end)
		return GIT_EOBJCORRUPTED;

	if (memcmp(buffer, header, header_len) != 0)
		return GIT_EOBJCORRUPTED;

	buffer += header_len;

	/* Parse name */
	if ((name_end = memchr(buffer, '<', buffer_end - buffer)) == NULL)
		return GIT_EOBJCORRUPTED;

	name_length = name_end - buffer - 1;
	sig->name = git__malloc(name_length + 1);
	if (sig->name == NULL)
		return GIT_ENOMEM;

	memcpy(sig->name, buffer, name_length);
	sig->name[name_length] = 0;
	buffer = name_end + 1;

	if (buffer >= line_end)
		return GIT_EOBJCORRUPTED;

	/* Parse email */
	if ((email_end = memchr(buffer, '>', buffer_end - buffer)) == NULL)
		return GIT_EOBJCORRUPTED;

	email_length = email_end - buffer;
	sig->email = git__malloc(email_length + 1);
	if (sig->name == NULL)
		return GIT_ENOMEM;

	memcpy(sig->email, buffer, email_length);
	sig->email[email_length] = 0;
	buffer = email_end + 1;

	if (buffer >= line_end)
		return GIT_EOBJCORRUPTED;

	if (git__strtol32(&time, buffer, &buffer, 10) < GIT_SUCCESS)
		return GIT_EOBJCORRUPTED;

	sig->when.time = (time_t)time;

	if (parse_timezone_offset(buffer, &offset) < GIT_SUCCESS)
		return GIT_EOBJCORRUPTED;
	
	sig->when.offset = offset;

	*buffer_out = (line_end + 1);
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


