/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "config_entries.h"

typedef struct config_entry_list {
	struct config_entry_list *next;
	struct config_entry_list *last;
	git_config_entry *entry;
} config_entry_list;

typedef struct config_entries_iterator {
	git_config_iterator parent;
	git_config_entries *entries;
	config_entry_list *head;
} config_entries_iterator;

struct git_config_entries {
	git_refcount rc;
	git_strmap *map;
	config_entry_list *list;
};

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

int git_config_entries_new(git_config_entries **out)
{
	git_config_entries *entries;
	int error;

	entries = git__calloc(1, sizeof(git_config_entries));
	GITERR_CHECK_ALLOC(entries);
	GIT_REFCOUNT_INC(entries);

	if ((error = git_strmap_alloc(&entries->map)) < 0)
		git__free(entries);
	else
		*out = entries;

	return error;
}

int git_config_entries_dup(git_config_entries **out, git_config_entries *entries)
{
	git_config_entries *result = NULL;
	config_entry_list *head;
	int error;

	if ((error = git_config_entries_new(&result)) < 0)
		goto out;

	for (head = entries->list; head; head = head->next) {
		git_config_entry *dup;

		dup = git__calloc(1, sizeof(git_config_entry));
		dup->name = git__strdup(head->entry->name);
		GITERR_CHECK_ALLOC(dup->name);
		if (head->entry->value) {
			dup->value = git__strdup(head->entry->value);
			GITERR_CHECK_ALLOC(dup->value);
		}
		dup->level = head->entry->level;
		dup->include_depth = head->entry->include_depth;

		if ((error = git_config_entries_append(result, dup)) < 0)
			goto out;
	}

	*out = result;
	result = NULL;

out:
	git_config_entries_free(result);
	return error;
}

void git_config_entries_incref(git_config_entries *entries)
{
	GIT_REFCOUNT_INC(entries);
}

static void config_entries_free(git_config_entries *entries)
{
	config_entry_list *list = NULL, *next;

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

void git_config_entries_free(git_config_entries *entries)
{
	if (entries)
		GIT_REFCOUNT_DEC(entries, config_entries_free);
}

int git_config_entries_append(git_config_entries *entries, git_config_entry *entry)
{
	config_entry_list *existing, *var;
	int error = 0;
	size_t pos;

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

int config_entry_get(config_entry_list **out, git_config_entries *entries, const char *key)
{
	size_t pos;

	pos = git_strmap_lookup_index(entries->map, key);

	/* no error message; the config system will write one */
	if (!git_strmap_valid_index(entries->map, pos))
		return GIT_ENOTFOUND;

	*out = git_strmap_value_at(entries->map, pos);

	return 0;
}

int git_config_entries_get(git_config_entry **out, git_config_entries *entries, const char *key)
{
	config_entry_list *entry;
	int error;

	if ((error = config_entry_get(&entry, entries, key)) < 0)
		return error;
	*out = entry->last->entry;

	return 0;
}

int git_config_entries_get_unique(git_config_entry **out, git_config_entries *entries, const char *key)
{
	config_entry_list *entry;
	int error;

	if ((error = config_entry_get(&entry, entries, key)) < 0)
		return error;

	if (entry->next != NULL) {
		giterr_set(GITERR_CONFIG, "entry is not unique due to being a multivar");
		return -1;
	}

	if (entry->entry->include_depth) {
		giterr_set(GITERR_CONFIG, "entry is not unique due to being included");
		return -1;
	}

	*out = entry->entry;

	return 0;
}

void config_iterator_free(git_config_iterator *iter)
{
	config_entries_iterator *it = (config_entries_iterator *) iter;
	git_config_entries_free(it->entries);
	git__free(it);
}

int config_iterator_next(
	git_config_entry **entry,
	git_config_iterator *iter)
{
	config_entries_iterator *it = (config_entries_iterator *) iter;

	if (!it->head)
		return GIT_ITEROVER;

	*entry = it->head->entry;
	it->head = it->head->next;

	return 0;
}

int git_config_entries_iterator_new(git_config_iterator **out, git_config_entries *entries)
{
	config_entries_iterator *it;

	it = git__calloc(1, sizeof(config_entries_iterator));
	GITERR_CHECK_ALLOC(it);
	it->parent.next = config_iterator_next;
	it->parent.free = config_iterator_free;
	it->head = entries->list;
	it->entries = entries;

	git_config_entries_incref(entries);
	*out = &it->parent;

	return 0;
}
