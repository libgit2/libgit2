#include "clar_libgit2.h"
#include "status_helpers.h"

int cb_status__normal(
	const char *path, unsigned int status_flags, void *payload)
{
	status_entry_counts *counts = payload;

	if (counts->entry_count >= counts->expected_entry_count) {
		counts->wrong_status_flags_count++;
		goto exit;
	}

	if (strcmp(path, counts->expected_paths[counts->entry_count])) {
		counts->wrong_sorted_path++;
		goto exit;
	}

	if (status_flags != counts->expected_statuses[counts->entry_count])
		counts->wrong_status_flags_count++;

exit:
	counts->entry_count++;
	return 0;
}

int cb_status__count(const char *p, unsigned int s, void *payload)
{
	volatile int *count = (int *)payload;

	GIT_UNUSED(p);
	GIT_UNUSED(s);

	(*count)++;

	return 0;
}

int cb_status__single(const char *p, unsigned int s, void *payload)
{
	status_entry_single *data = (status_entry_single *)payload;

	GIT_UNUSED(p);

	data->count++;
	data->status = s;

	return 0;
}
