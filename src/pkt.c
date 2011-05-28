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
	const char *num_end;
	git_pkt *pkt;

	error = git__strtol32(&len, line, &num_end, 16);
	if (error < GIT_SUCCESS)
		return error;

	line = num_end;
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

	/* TODO: Write the rest of this thing */

	return GIT_SUCCESS;
}
