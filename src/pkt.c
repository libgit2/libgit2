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

#include "git2/pkt.h"
#include "git2/types.h"
#include "git2/errors.h"

#include "common.h"
#include "util.h"

static int flush_pkt(git_pkt **out)
{
	git_pkt *pkt;

	pkt = git__malloc(sizeof(git_pkt));
	if (pkt == NULL)
		return GIT_ENOMEM;

	pkt->type = GIT_PKT_FLUSH;
	*out = pkt;

	return GIT_SUCCESS;
}

/*
 * Parse an other-ref line.
 */
int ref_pkt(git_pkt **out, const char *line, size_t len)
{
	git_pkt_ref *pkt;
	int error;
	size_t name_len;

	pkt = git__malloc(sizeof(git_pkt_ref));
	if (pkt == NULL)
		return GIT_ENOMEM;

	memset(pkt, 0x0, sizeof(git_pkt_ref));
	pkt->type = GIT_PKT_REF;
	error = git_oid_fromstr(&pkt->head.oid, line);
	if (error < GIT_SUCCESS) {
		error = git__throw(error, "Failed to parse reference ID");
		goto out;
	}

	/* Check for a bit of consistency */
	if (line[GIT_OID_HEXSZ] != ' ') {
		error = git__throw(GIT_EOBJCORRUPTED, "Failed to parse ref. No SP");
		goto out;
	}

	/* Jump from the name */
	line += GIT_OID_HEXSZ + 1;
	len -= (GIT_OID_HEXSZ + 1);

	name_len = min(strlen(line), len);
	if (line[name_len - 1] == '\n')
		--name_len;

	pkt->head.name = git__strndup(line, name_len);
	if (pkt->head.name == NULL) {
		error = GIT_ENOMEM;
		goto out;
	}

	/* Try to get the capabilities */
	line += name_len + 1; /* + \0 */
	len -= (name_len + 1);
	if (line[len - 1] == '\n')
		--len;

	if (len > 0) { /* capatilities */
		pkt->capabilities = git__malloc(len);
		if (pkt->capabilities == NULL) {
			error = GIT_ENOMEM;
			goto out;
		}

		memcpy(pkt->capabilities, line, len);
	}

out:
	if (error < GIT_SUCCESS)
		free(pkt);
	else
		*out = (git_pkt *)pkt;

	return error;
}

/*
 * As per the documentation, the syntax is:
 *
 * pkt-line    = data-pkt / flush-pkt
 * data-pkt    = pkt-len pkt-payload
 * pkt-len     = 4*(HEXDIG)
 * pkt-payload = (pkt-len -4)*(OCTET)
 * flush-pkt   = "0000"
 *
 * Which means that the first four bytes are the length of the line,
 * in ASCII hexadecimal (including itself)
 */

int git_pkt_parse_line(git_pkt **head, const char *line, const char **out)
{
	int error = GIT_SUCCESS;
	long int len;
	const int num_len = 4;
	char *num;
	const char *num_end;

	num = git__strndup(line, num_len);
	if (num == NULL)
		return GIT_ENOMEM;

	error = git__strtol32(&len, num, &num_end, 16);
	if (error < GIT_SUCCESS) {
		free(num);
		return git__throw(error, "Failed to parse pkt length");
	}
	if (num_end - num != num_len) {
		free(num);
		return git__throw(GIT_EOBJCORRUPTED, "Wrong pkt length");
	}
	free(num);

	line += num_len;
	/*
	 * TODO: How do we deal with empty lines? Try again? with the next
	 * line?
	 */
	if (len == 4) {
		*out = line;
		return GIT_SUCCESS;
	}

	if (len == 0) { /* Flush pkt */
		*out = line;
		return flush_pkt(head);
	}

	len -= num_len; /* the length includes the space for the length */

	/*
	 * For now, we're just going to assume we're parsing references
	 */

	error = ref_pkt(head, line, len);
	*out = line + len;

	return error;
}

/*
 * Create a git procol request.
 *
 * For example: 0035git-upload-pack /libgit2/libgit2\0host=github.com\0
 *
 * TODO: the command should not be hard-coded
 */
int git_pkt_gen_proto(char **out, int *outlen, const char *url)
{
	char *delim, *repo, *ptr;
	char command[] = "git-upload-pack";
	char host[] = "host=";
	int len;

	delim = strchr(url, '/');
	if (delim == NULL)
		return git__throw(GIT_EOBJCORRUPTED, "Failed to create proto-request: malformed URL");

	repo = delim;

	delim = strchr(url, ':');
	if (delim == NULL)
		delim = strchr(url, '/');

	len = 4 + STRLEN(command) + 1 + strlen(repo) + 1 + STRLEN(host) + (delim - url) + 2;

	*out = git__malloc(len);
	if (*out == NULL)
		return GIT_ENOMEM;

	*outlen = len - 1;
	ptr = *out;
	memset(ptr, 0x0, len);
	/* We expect the return value to be > len - 1 so don't bother checking it */
	snprintf(ptr, len -1, "%04x%s %s%c%s%s", len - 1, command, repo, 0, host, url);

	return GIT_SUCCESS;
}
