#include "git2.h"
#include "vector.h"
#include "backends.h"

static git_vector odb_registrations = GIT_VECTOR_INIT;

git_odb_registration *git_odb_registration__find(const char *name)
{
	size_t i;
	git_odb_registration *reg;

	git_vector_foreach(&odb_registrations, i, reg) {
		if (!strcmp(name, reg->name))
			return reg;
	}

	return NULL;
}

int git_odb_register(const char *name, git_odb_ctor ctor, void *payload)
{
	int error;
	git_odb_registration *reg;

	reg = git__calloc(1, sizeof(git_odb_registration));
	GITERR_CHECK_ALLOC(reg);

	reg->name = git__strdup(name);
	GITERR_CHECK_ALLOC(reg->name);

	reg->ctor = ctor;
	reg->payload = payload;

	if ((error = git_vector_insert(&odb_registrations, reg)) < 0)
		goto on_error;

	return 0;

on_error:
	git__free(reg->name);
	git__free(reg);
	return error;
}
