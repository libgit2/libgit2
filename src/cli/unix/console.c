#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>

#include "cli.h"

int cli_console_coords(int *cols, int *rows, int fd)
{
	struct winsize ws;

	if (isatty(fd) == 0 && errno != ENOTTY) {
		git_error_set(GIT_ERROR_OS, "failed to query window size");
		return -1;
	}

	if (ioctl(fd, TIOCGWINSZ, &ws) < 0) {
		git_error_set(GIT_ERROR_OS, "failed to query window size");
		return -1;
	}

	if (cols)
		*cols = ws.ws_col;

	if (rows)
		*rows = ws.ws_row;

	return 0;
}
