#include <git2.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

enum print_options {
	SKIP = 1,
	VERBOSE = 2,
};

struct print_payload {
	enum print_options options;
	git_repository *repo;
};

void init_array(git_strarray *array, int argc, char **argv)
{
	unsigned int i;

	array->count = argc;
	array->strings = malloc(sizeof(char*) * array->count);
	assert(array->strings!=NULL);

	for(i=0; i<array->count; i++) {
		array->strings[i]=argv[i];
	}

	return;
}

int print_matched_cb(const char *path, const char *matched_pathspec, void *payload)
{
	(void)matched_pathspec;

	struct print_payload p = *(struct print_payload*)(payload);
	int ret;
	git_status_t status;

	if (git_status_file(&status, p.repo, path)) {
		return -1; //abort
	}

	if (status & GIT_STATUS_WT_MODIFIED ||
	         status & GIT_STATUS_WT_NEW) {
		if (p.options & VERBOSE || p.options & SKIP) {
			printf("add '%s'\n", path);
		}
		ret = 0;
	} else {
		ret = 1;
	}

	if(p.options & SKIP) {
		ret = 1;
	}

	return ret;
}

int main (int argc, char** argv)
{
	git_index_matched_path_cb matched_cb = NULL;
	git_repository *repo = NULL;
	git_index *index;
	git_strarray array = {0};
	int i, options = 0;
	struct print_payload payload = {0};

	if (argc < 2) {
		fprintf(stderr, "usage: add file-spec [file-spec] [...]\n");
		return 1;
	}

	for (i = 1; i < argc; ++i) {
		if (argv[i][0] != '-') {
			break;
		}
		else if(!strcmp(argv[i], "--verbose") || !strcmp(argv[i], "-v")) {
			options |= VERBOSE;
		}
		else if(!strcmp(argv[i], "--dry-run") || !strcmp(argv[i], "-n")) {
			options |= SKIP;
		}
		else if(!strcmp(argv[i], "--")) {
			break;
		}
		else {
			fprintf(stderr, "Unsupported option %s.\n", argv[i]);
			return 1;
		}
	}
	
	init_array(&array, argc-i, argv+i);

	printf("args:\n");
	for(i=0; i<array.count; i++) {
		printf(" - %s\n", array.strings[i]);
	}

	if (git_repository_open(&repo, ".") < 0) {
		fprintf(stderr, "No git repository\n");
		return 1;
	}

	if (git_repository_index(&index, repo) < 0) {
		fprintf(stderr, "Could not open repository index\n");
		return 1;
	}

	matched_cb = &print_matched_cb;

	payload.options = options;
	payload.repo = repo;
	git_index_add_all(index, &array, 0, matched_cb, &payload);

	git_index_write(index);
	git_index_free(index);
	git_repository_free(repo);

	return 0;
}
