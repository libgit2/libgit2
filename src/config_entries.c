/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "config_entries.h"

static void config_entry_list_free(config_entry_list *list)
{
	config_entry_list *next;

	while (list != NULL) {
		next = list->next;

		git__free((char*) list->entry->name);
		git__free((char *) list->entry->value);
		git__free(list->entry);
		git__free(list);

		list = next;
	};
}

static void config_entry_list_append(config_entry_list **list, config_entry_list *entry)
{
	if (*list)
		(*list)->last->next = entry;
	else
		*list = entry;
	(*list)->last = entry;
}

int diskfile_entries_alloc(diskfile_entries **out)
{
	diskfile_entries *entries;
	int error;

	entries = git__calloc(1, sizeof(diskfile_entries));
	GITERR_CHECK_ALLOC(entries);

	git_atomic_set(&entries->refcount, 1);

	if ((error = git_strmap_alloc(&entries->map)) < 0)
		git__free(entries);
	else
		*out = entries;

	return error;
}

void diskfile_entries_free(diskfile_entries *entries)
{
	config_entry_list *list = NULL, *next;

	if (!entries)
		return;

	if (git_atomic_dec(&entries->refcount) != 0)
		return;

	git_strmap_foreach_value(entries->map, list, config_entry_list_free(list));
	git_strmap_free(entries->map);

	list = entries->list;
	while (list != NULL) {
		next = list->next;
		git__free(list);
		list = next;
	}

	git__free(entries);
}

int diskfile_entries_append(diskfile_entries *entries, git_config_entry *entry)
{
	git_strmap_iter pos;
	config_entry_list *existing, *var;
	int error = 0;

	var = git__calloc(1, sizeof(config_entry_list));
	GITERR_CHECK_ALLOC(var);
	var->entry = entry;

	pos = git_strmap_lookup_index(entries->map, entry->name);
	if (!git_strmap_valid_index(entries->map, pos)) {
		/*
		 * We only ever inspect `last` from the first config
		 * entry in a multivar. In case where this new entry is
		 * the first one in the entry map, it will also be the
		 * last one at the time of adding it, which is
		 * why we set `last` here to itself. Otherwise we
		 * do not have to set `last` and leave it set to
		 * `NULL`.
		 */
		var->last = var;

		git_strmap_insert(entries->map, entry->name, var, &error);

		if (error > 0)
			error = 0;
	} else {
		existing = git_strmap_value_at(entries->map, pos);
		config_entry_list_append(&existing, var);
	}

	var = git__calloc(1, sizeof(config_entry_list));
	GITERR_CHECK_ALLOC(var);
	var->entry = entry;
	config_entry_list_append(&entries->list, var);

	return error;
}

void config_iterator_free(
	git_config_iterator* iter)
{
	iter->backend->free(iter->backend);
	git__free(iter);
}

int config_iterator_next(
	git_config_entry **entry,
	git_config_iterator *iter)
{
	git_config_file_iter *it = (git_config_file_iter *) iter;

	if (!it->head)
		return GIT_ITEROVER;

	*entry = it->head->entry;
	it->head = it->head->next;

	return 0;
}
