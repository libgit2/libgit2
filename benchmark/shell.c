#include <stdio.h>
#include "git2.h"
#include "git2/sys/repository.h"
#include "common.h"
#include "bench_util.h"
#include "buffer.h"
#include "fileops.h"
#include "opt.h"
#include "run.h"
#include "operation.h"
#include "benchmark.h"
#include "shell.h"

#if defined(_WIN32)

extern int gitbench_shell(
	const char * const argv[],
	const char *new_cwd,
	int *p_raw_exit_status)
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	DWORD dw;
	git_buf buf_cl = GIT_BUF_INIT;
	int k;
	bool ok = false;

	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	memset(&pi, 0, sizeof(pi));

	/* TODO decide if we need to re-quote the args. */
	for (k = 0; argv[k]; k++) {
		const char *pk = argv[k];
		git_buf_puts(&buf_cl, pk);
		git_buf_putc(&buf_cl, ' ');
	}

	if (verbosity)
		fprintf(logfile, "::::: %s\n", buf_cl.ptr);

	if (!CreateProcessA(NULL, buf_cl.ptr, NULL, NULL, TRUE,
						0, NULL, new_cwd, &si, &pi)) {
		fprintf(stderr, "Error[0x%08lx] CreateProcessA: %s\n",
				GetLastError(), buf_cl.ptr);
		goto done;
	}

	if (WaitForSingleObject(pi.hProcess, INFINITE) != WAIT_OBJECT_0) {
		fprintf(stderr, "Error[0x%08lx] Wait failed\n",
				GetLastError());
		goto done;
	}

	if (!GetExitCodeProcess(pi.hProcess, &dw)) {
		fprintf(stderr, "Error[0x%08lx] GetExitCodeProcess failed\n",
				GetLastError());
		goto done;
	}

	if (p_raw_exit_status)
		*p_raw_exit_status = (int)dw;

	ok = (dw == 0);

done:
	if (pi.hThread != INVALID_HANDLE_VALUE)
		CloseHandle(pi.hThread);
	if (pi.hProcess != INVALID_HANDLE_VALUE)
		CloseHandle(pi.hProcess);
	git_buf_free(&buf_cl);
	return ((ok) ? 0 : -1);
}

#else

/* Non-windows version adapted from tests/clar/fs.h:shell_out() */

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

extern int gitbench_shell(
	const char * const argv[],
	const char *new_cwd,
	int *p_raw_exit_status)
{
	int status, piderr;
	int raw_exit_status;
	bool ok = false;
	pid_t pid;

	pid = fork();

	if (pid < 0) {
		fprintf(stderr,
			"System error: `fork()` call failed (%d) - %s\n",
			errno, strerror(errno));
		goto done;
	}

	if (pid == 0) {
		if (new_cwd && (p_chdir(new_cwd) != 0))
			exit(-1);
		exit(execv(argv[0], (char **)argv));
	}

	do {
		piderr = waitpid(pid, &status, WUNTRACED);
	} while (piderr < 0 && (errno == EAGAIN || errno == EINTR));

	raw_exit_status = WEXITSTATUS(status);
	if (p_raw_exit_status)
		*p_raw_exit_status = raw_exit_status;

	ok = (raw_exit_status == 0);

done:
	return ((ok) ? 0 : -1);
}

#endif /* !_WIN32 */
