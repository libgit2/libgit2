#include "common.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

/* Shamelessly borrowed from http://stackoverflow.com/questions/3417837/
 * with permission of the original author, Martin Pool.
 * http://sourcefrog.net/weblog/software/languages/C/unused.html
 */
#ifdef UNUSED
#elif defined(__GNUC__)
# define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#elif defined(__LCLINT__)
# define UNUSED(x) /*@unused@*/ x
#else
# define UNUSED(x) x
#endif

static int readline(char **out)
{
	int c, error = 0, length = 0, allocated = 0;
	char *line = NULL;

	errno = 0;

	while ((c = getchar()) != EOF) {
		if (length == allocated) {
			allocated += 16;

			if ((line = realloc(line, allocated)) == NULL) {
				error = -1;
				goto error;
			}
		}

		if (c == '\n')
			break;

		line[length++] = c;
	}

	if (errno != 0) {
		error = -1;
		goto error;
	}

	line[length] = '\0';
	*out = line;
	line = NULL;
	error = length;
error:
	free(line);
	return error;
}

int cred_acquire_cb(git_cred **out,
		const char * UNUSED(url),
		const char * UNUSED(username_from_url),
		unsigned int UNUSED(allowed_types),
		void * UNUSED(payload))
{
	char *username = NULL, *password = NULL;
	int error;

	printf("Username: ");
	if (readline(&username) < 0) {
		fprintf(stderr, "Unable to read username: %s", strerror(errno));
		return -1;
	}

	/* Yup. Right there on your terminal. Careful where you copy/paste output. */
	printf("Password: ");
	if (readline(&password) < 0) {
		fprintf(stderr, "Unable to read password: %s", strerror(errno));
		free(username);
		return -1;
	}

	error = git_cred_userpass_plaintext_new(out, username, password);

	free(username);
	free(password);

	return error;
}
