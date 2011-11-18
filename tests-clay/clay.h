#ifndef __CLAY_TEST_H__
#define __CLAY_TEST_H__

#include <stdlib.h>

void clay__assert(
	int condition,
	const char *file,
	int line,
	const char *error,
	const char *description,
	int should_abort);

void cl_set_cleanup(void (*cleanup)(void *), void *opaque);
void cl_fs_cleanup(void);

#ifdef CLAY_FIXTURE_PATH
const char *cl_fixture(const char *fixture_name);
void cl_fixture_sandbox(const char *fixture_name);
void cl_fixture_cleanup(const char *fixture_name);
#endif

/**
 * Assertion macros with explicit error message
 */
#define cl_must_pass_(expr, desc) clay__assert((expr) >= 0, __FILE__, __LINE__, "Function call failed: " #expr, desc, 1)
#define cl_must_fail_(expr, desc) clay__assert((expr) < 0, __FILE__, __LINE__, "Expected function call to fail: " #expr, desc, 1)
#define cl_assert_(expr, desc) clay__assert((expr) != 0, __FILE__, __LINE__, "Expression is not true: " #expr, desc, 1)

/**
 * Check macros with explicit error message
 */
#define cl_check_pass_(expr, desc) clay__assert((expr) >= 0, __FILE__, __LINE__, "Function call failed: " #expr, desc, 0)
#define cl_check_fail_(expr, desc) clay__assert((expr) < 0, __FILE__, __LINE__, "Expected function call to fail: " #expr, desc, 0)
#define cl_check_(expr, desc) clay__assert((expr) != 0, __FILE__, __LINE__, "Expression is not true: " #expr, desc, 0)

/**
 * Assertion macros with no error message
 */
#define cl_must_pass(expr) cl_must_pass_(expr, NULL)
#define cl_must_fail(expr) cl_must_fail_(expr, NULL)
#define cl_assert(expr) cl_assert_(expr, NULL)

/**
 * Check macros with no error message
 */
#define cl_check_pass(expr) cl_check_pass_(expr, NULL)
#define cl_check_fail(expr) cl_check_fail_(expr, NULL)
#define cl_check(expr) cl_check_(expr, NULL)

/**
 * Forced failure/warning
 */
#define cl_fail(desc) clay__assert(0, __FILE__, __LINE__, "Test failed.", desc, 1)
#define cl_warning(desc) clay__assert(0, __FILE__, __LINE__, "Warning during test execution:", desc, 0)

/**
 * Test method declarations
 */
extern void test_config_stress__cleanup(void);
extern void test_config_stress__dont_break_on_invalid_input(void);
extern void test_config_stress__initialize(void);
extern void test_core_dirent__dont_traverse_dot(void);
extern void test_core_dirent__dont_traverse_empty_folders(void);
extern void test_core_dirent__traverse_slash_terminated_folder(void);
extern void test_core_dirent__traverse_subfolder(void);
extern void test_core_dirent__traverse_weird_filenames(void);
extern void test_core_filebuf__0(void);
extern void test_core_filebuf__1(void);
extern void test_core_filebuf__2(void);
extern void test_core_oid__initialize(void);
extern void test_core_oid__streq(void);
extern void test_core_path__0(void);
extern void test_core_path__1(void);
extern void test_core_path__2(void);
extern void test_core_path__5(void);
extern void test_core_path__6(void);
extern void test_core_rmdir__delete_recursive(void);
extern void test_core_rmdir__fail_to_delete_non_empty_dir(void);
extern void test_core_rmdir__initialize(void);
extern void test_core_string__0(void);
extern void test_core_string__1(void);
extern void test_core_strtol__int32(void);
extern void test_core_strtol__int64(void);
extern void test_core_vector__0(void);
extern void test_core_vector__1(void);
extern void test_core_vector__2(void);
extern void test_index_rename__single_file(void);
extern void test_network_remotes__cleanup(void);
extern void test_network_remotes__fnmatch(void);
extern void test_network_remotes__initialize(void);
extern void test_network_remotes__parsing(void);
extern void test_network_remotes__refspec_parsing(void);
extern void test_network_remotes__transform(void);
extern void test_object_raw_chars__build_valid_oid_from_raw_bytes(void);
extern void test_object_raw_chars__find_invalid_chars_in_oid(void);
extern void test_object_raw_compare__compare_allocfmt_oids(void);
extern void test_object_raw_compare__compare_fmt_oids(void);
extern void test_object_raw_compare__compare_pathfmt_oids(void);
extern void test_object_raw_compare__succeed_on_copy_oid(void);
extern void test_object_raw_compare__succeed_on_oid_comparison_equal(void);
extern void test_object_raw_compare__succeed_on_oid_comparison_greater(void);
extern void test_object_raw_compare__succeed_on_oid_comparison_lesser(void);
extern void test_object_raw_convert__succeed_on_oid_to_string_conversion(void);
extern void test_object_raw_convert__succeed_on_oid_to_string_conversion_big(void);
extern void test_object_raw_fromstr__fail_on_invalid_oid_string(void);
extern void test_object_raw_fromstr__succeed_on_valid_oid_string(void);
extern void test_object_raw_hash__hash_buffer_in_single_call(void);
extern void test_object_raw_hash__hash_by_blocks(void);
extern void test_object_raw_hash__hash_commit_object(void);
extern void test_object_raw_hash__hash_junk_data(void);
extern void test_object_raw_hash__hash_multi_byte_object(void);
extern void test_object_raw_hash__hash_one_byte_object(void);
extern void test_object_raw_hash__hash_tag_object(void);
extern void test_object_raw_hash__hash_tree_object(void);
extern void test_object_raw_hash__hash_two_byte_object(void);
extern void test_object_raw_hash__hash_vector(void);
extern void test_object_raw_hash__hash_zero_length_object(void);
extern void test_object_raw_short__oid_shortener_no_duplicates(void);
extern void test_object_raw_short__oid_shortener_stresstest_git_oid_shorten(void);
extern void test_object_raw_size__validate_oid_size(void);
extern void test_object_raw_type2string__check_type_is_loose(void);
extern void test_object_raw_type2string__convert_string_to_type(void);
extern void test_object_raw_type2string__convert_type_to_string(void);
extern void test_object_tree_frompath__cleanup(void);
extern void test_object_tree_frompath__fail_when_processing_an_invalid_path(void);
extern void test_object_tree_frompath__fail_when_processing_an_unknown_tree_segment(void);
extern void test_object_tree_frompath__initialize(void);
extern void test_object_tree_frompath__retrieve_tree_from_path_to_treeentry(void);
extern void test_status_single__hash_single_file(void);
extern void test_status_worktree__cleanup(void);
extern void test_status_worktree__empty_repository(void);
extern void test_status_worktree__initialize(void);
extern void test_status_worktree__whole_repository(void);

#endif
