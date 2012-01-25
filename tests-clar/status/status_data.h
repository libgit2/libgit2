
struct status_entry_counts {
	int wrong_status_flags_count;
	int wrong_sorted_path;
	int entry_count;
	const unsigned int* expected_statuses;
	const char** expected_paths;
	int expected_entry_count;
};

static const char *entry_paths0[] = {
	"file_deleted",
	"ignored_file",
	"modified_file",
	"new_file",
	"staged_changes",
	"staged_changes_file_deleted",
	"staged_changes_modified_file",
	"staged_delete_file_deleted",
	"staged_delete_modified_file",
	"staged_new_file",
	"staged_new_file_deleted_file",
	"staged_new_file_modified_file",

	"subdir/deleted_file",
	"subdir/modified_file",
	"subdir/new_file",
};

static const unsigned int entry_statuses0[] = {
	GIT_STATUS_WT_DELETED,
	GIT_STATUS_IGNORED,
	GIT_STATUS_WT_MODIFIED,
	GIT_STATUS_WT_NEW,
	GIT_STATUS_INDEX_MODIFIED,
	GIT_STATUS_INDEX_MODIFIED | GIT_STATUS_WT_DELETED,
	GIT_STATUS_INDEX_MODIFIED | GIT_STATUS_WT_MODIFIED,
	GIT_STATUS_INDEX_DELETED,
	GIT_STATUS_INDEX_DELETED | GIT_STATUS_WT_NEW,
	GIT_STATUS_INDEX_NEW,
	GIT_STATUS_INDEX_NEW | GIT_STATUS_WT_DELETED,
	GIT_STATUS_INDEX_NEW | GIT_STATUS_WT_MODIFIED,

	GIT_STATUS_WT_DELETED,
	GIT_STATUS_WT_MODIFIED,
	GIT_STATUS_WT_NEW,
};

static const size_t entry_count0 = 15;

