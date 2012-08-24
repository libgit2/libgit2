#include "clar_libgit2.h"
#include "buffer.h"
#include "path.h"
#include "util.h"
#include "posix.h"
#include "submodule_helpers.h"

/* rewrite gitmodules -> .gitmodules
 * rewrite the empty or relative urls inside each module
 * rename the .gitted directory inside any submodule to .git
 */
void rewrite_gitmodules(const char *workdir)
{
	git_buf in_f = GIT_BUF_INIT, out_f = GIT_BUF_INIT, path = GIT_BUF_INIT;
	FILE *in, *out;
	char line[256];

	cl_git_pass(git_buf_joinpath(&in_f, workdir, "gitmodules"));
	cl_git_pass(git_buf_joinpath(&out_f, workdir, ".gitmodules"));

	cl_assert((in  = fopen(in_f.ptr, "r")) != NULL);
	cl_assert((out = fopen(out_f.ptr, "w")) != NULL);

	while (fgets(line, sizeof(line), in) != NULL) {
		char *scan = line;

		while (*scan == ' ' || *scan == '\t') scan++;

		/* rename .gitted -> .git in submodule directories */
		if (git__prefixcmp(scan, "path =") == 0) {
			scan += strlen("path =");
			while (*scan == ' ') scan++;

			git_buf_joinpath(&path, workdir, scan);
			git_buf_rtrim(&path);
			git_buf_joinpath(&path, path.ptr, ".gitted");

			if (!git_buf_oom(&path) && p_access(path.ptr, F_OK) == 0) {
				git_buf_joinpath(&out_f, workdir, scan);
				git_buf_rtrim(&out_f);
				git_buf_joinpath(&out_f, out_f.ptr, ".git");

				if (!git_buf_oom(&out_f))
					p_rename(path.ptr, out_f.ptr);
			}
		}

		/* copy non-"url =" lines verbatim */
		if (git__prefixcmp(scan, "url =") != 0) {
			fputs(line, out);
			continue;
		}

		/* convert relative URLs in "url =" lines */
		scan += strlen("url =");
		while (*scan == ' ') scan++;

		if (*scan == '.') {
			git_buf_joinpath(&path, workdir, scan);
			git_buf_rtrim(&path);
		} else if (!*scan || *scan == '\n') {
			git_buf_joinpath(&path, workdir, "../testrepo.git");
		} else {
			fputs(line, out);
			continue;
		}

		git_path_prettify(&path, path.ptr, NULL);
		git_buf_putc(&path, '\n');
		cl_assert(!git_buf_oom(&path));

		fwrite(line, scan - line, sizeof(char), out);
		fputs(path.ptr, out);
	}

	fclose(in);
	fclose(out);

	cl_must_pass(p_unlink(in_f.ptr));

	git_buf_free(&in_f);
	git_buf_free(&out_f);
	git_buf_free(&path);
}
