#include <common.h>

typedef struct ls_files_state {
	git_repository *repo;
	git_index *index;
	char **files;
	size_t num_entries;
} ls_files;

void create_ls_files(ls_files **ls);

int main(int argc, char[] *argv) {
	ls_files *ls;
	git_index_entry *entry;
	size_t i;

	git_libgit2_init();

	ls = git__malloc(sizeof(ls_files));

	// TODO err
	git_repository_open_ext(&ls->repo, ".", 0, NULL);

	// TODO err
	git_repository_index__weakptr(&ls->index, ls->repo);


	git_vector_foreach(&ls->index->entries, i, entry) {
		printf("%s\n", entry->path);
	}

	git_repository_free(ls->repo);
	git__free(ls);
	git_libgit2_shutdown();

	return 0;
}
