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
#include "person.h"
#include "repository.h"
#include "git2/common.h"

void git_person__free(git_person *person)
{
	if (person == NULL)
		return;

	free(person->name);
	free(person->email);
	free(person);
}

git_person *git_person__new(const char *name, const char *email, time_t time, int offset)
{
	git_person *p;

	if ((p = git__malloc(sizeof(git_person))) == NULL)
		goto cleanup;

	p->name = git__strdup(name);
	p->email = git__strdup(email);
	p->time = time;
	p->timezone_offset = offset;

	if (p->name == NULL || p->email == NULL)
		goto cleanup;

	return p;

cleanup:
	git_person__free(p);
	return NULL;
}

const char *git_person_name(git_person *person)
{
	return person->name;
}

const char *git_person_email(git_person *person)
{
	return person->email;
}

time_t git_person_time(git_person *person)
{
	return person->time;
}

int git_person_timezone_offset(git_person *person)
{
	return person->timezone_offset;
}

int git_person__parse_timezone_offset(const char *buffer, int *offset_out)
{
	int offset, dec_offset;
	int mins, hours;

	const char* offset_start;
	char* offset_end;

	offset_start = buffer + 1;

	if (offset_start[0] != '-' && offset_start[0] != '+')
		return GIT_EOBJCORRUPTED;

	dec_offset = strtol(offset_start + 1, &offset_end, 10);

	if (offset_end - offset_start != 5)
		return GIT_EOBJCORRUPTED;

	hours = dec_offset / 100;
	mins = dec_offset % 100;

	if (hours > 14)			// see http://www.worldtimezone.com/faq.html 
		return GIT_EOBJCORRUPTED;

	if (mins > 59) 
		return GIT_EOBJCORRUPTED;

	offset = (hours * 60) + mins;

	if (offset_start[0] == '-')
	{
		offset *= -1;
	}
	
	*offset_out = offset;

	return GIT_SUCCESS;
}


int git_person__parse(git_person *person, char **buffer_out,
		const char *buffer_end, const char *header)
{
	const size_t header_len = strlen(header);

	int name_length, email_length;
	char *buffer = *buffer_out;
	char *line_end, *name_end, *email_end;
	int offset = 0;

	memset(person, 0x0, sizeof(git_person));

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
	person->name = git__malloc(name_length + 1);
	memcpy(person->name, buffer, name_length);
	person->name[name_length] = 0;
	buffer = name_end + 1;

	if (buffer >= line_end)
		return GIT_EOBJCORRUPTED;

	/* Parse email */
	if ((email_end = memchr(buffer, '>', buffer_end - buffer)) == NULL)
		return GIT_EOBJCORRUPTED;

	email_length = email_end - buffer;
	person->email = git__malloc(email_length + 1);
	memcpy(person->email, buffer, email_length);
	person->email[email_length] = 0;
	buffer = email_end + 1;

	if (buffer >= line_end)
		return GIT_EOBJCORRUPTED;

	person->time = strtol(buffer, &buffer, 10);

	if (person->time == 0)
		return GIT_EOBJCORRUPTED;

	if (git_person__parse_timezone_offset(buffer, &offset) < GIT_SUCCESS)
		return GIT_EOBJCORRUPTED;
	
	person->timezone_offset = offset;

	*buffer_out = (line_end + 1);
	return GIT_SUCCESS;
}

int git_person__write(git_odb_source *src, const char *header, const git_person *person)
{
	char *sign;
	int offset, hours, mins;

	offset = person->timezone_offset;
	sign = (person->timezone_offset < 0) ? "-" : "+";
	
	if (offset < 0)
		offset = -offset;

	hours = offset / 60;
	mins = offset % 60;

	return git__source_printf(src, "%s %s <%s> %u %s%02d%02d\n", header, person->name, person->email, person->time, sign, hours, mins);
}


