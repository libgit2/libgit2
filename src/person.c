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
#include "git/common.h"

void git_person__free(git_person *person)
{
	if (person == NULL)
		return;

	free(person->name);
	free(person->email);
	free(person);
}

git_person *git_person__new(const char *name, const char *email, time_t time)
{
	git_person *p;

	if ((p = git__malloc(sizeof(git_person))) == NULL)
		goto cleanup;

	p->name = git__strdup(name);
	p->email = git__strdup(email);
	p->time = time;

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

int git_person__parse(git_person *person, char **buffer_out,
		const char *buffer_end, const char *header)
{
	const size_t header_len = strlen(header);

	int name_length, email_length;
	char *buffer = *buffer_out;
	char *line_end, *name_end, *email_end;

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

	*buffer_out = (line_end + 1);
	return GIT_SUCCESS;
}

int git_person__write(git_odb_source *src, const char *header, const git_person *person)
{
	return git__source_printf(src, "%s %s <%s> %u\n", header, person->name, person->email, person->time);
}


