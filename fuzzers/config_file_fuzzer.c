#include <git2.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>

int foreach_cb(const git_config_entry *entry, void *payload)
{
	return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	static int fd = -1;
	static char path[] = "/tmp/git.XXXXXX";
	if (fd < 0) {
		git_libgit2_init();
		fd = mkstemp(path);
		if (fd < 0) {
			abort();
		}
	}
	if (ftruncate(fd, 0) !=0 ) {
		abort();
	}
	if (lseek(fd, 0, SEEK_SET) != 0) {
		abort();
	}
	if (write(fd, data, size) != size) {
		abort();
	}

	git_config *cfg;
	int err = git_config_open_ondisk(&cfg, path);
	if (err == 0) {
		git_config_foreach(cfg, foreach_cb, NULL);
		git_config_free(cfg);
	}

	return 0;
}
