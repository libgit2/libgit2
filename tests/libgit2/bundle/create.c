#include "clar_libgit2.h"

#include "bundle.h"
#include "futils.h"
#include "fs_path.h"
#include "posix.h"

#define BUNDLE_PATH "create_test.bundle"
#define CLONE_PATH "create_clone"

#define ANNOTATED_TAG_ID "7b4384978d2493e851f9cca7858815fac9b10980"
#define TAGGED_COMMIT_ID "e90810b8df3e80c413d903f631643c716887138d"

static git_repository *g_repo;
static git_repository *g_clone;

void test_bundle_create__initialize(void)
{
	g_repo = cl_git_sandbox_init("testrepo.git");
}

void test_bundle_create__cleanup(void)
{
	git_repository_free(g_clone);
	g_clone = NULL;

	cl_git_sandbox_cleanup();
	g_repo = NULL;

	cl_fixture_cleanup(BUNDLE_PATH);
	cl_fixture_cleanup(BUNDLE_PATH ".lock");
	cl_fixture_cleanup(CLONE_PATH);
	cl_fixture_cleanup("no_such_dir");
}

static void clone_bundle(void)
{
	git_clone_options opts = GIT_CLONE_OPTIONS_INIT;

	opts.bare = 1;
	cl_git_pass(git_clone(&g_clone, BUNDLE_PATH, CLONE_PATH, &opts));
}

static size_t count_history(git_repository *repo, const char *refname)
{
	git_revwalk *walk;
	git_oid id;
	size_t count = 0;

	cl_git_pass(git_revwalk_new(&walk, repo));
	cl_git_pass(git_revwalk_push_ref(walk, refname));

	while (git_revwalk_next(&id, walk) == 0)
		count++;

	git_revwalk_free(walk);
	return count;
}

void test_bundle_create__one_branch_clone(void)
{
	char *names[] = { "HEAD", "refs/heads/master" };
	git_strarray refs = { names, 2 };
	git_reference *src, *dst;
	size_t expected;

	cl_git_pass(git_bundle_create(g_repo, BUNDLE_PATH, &refs, NULL));
	clone_bundle();

	cl_git_pass(git_reference_lookup(&src, g_repo, "refs/heads/master"));
	cl_git_pass(git_reference_lookup(&dst, g_clone, "refs/remotes/origin/master"));
	cl_assert_equal_oid(git_reference_target(src), git_reference_target(dst));

	expected = count_history(g_repo, "refs/heads/master");
	cl_assert(expected > 1);
	cl_assert_equal_sz(expected,
		count_history(g_clone, "refs/remotes/origin/master"));

	git_reference_free(src);
	git_reference_free(dst);
}

void test_bundle_create__tag_only_includes_parents(void)
{
	char *names[] = { "refs/tags/e90810b" };
	git_strarray refs = { names, 1 };
	git_oid id;
	git_commit *commit;

	cl_git_pass(git_bundle_create(g_repo, BUNDLE_PATH, &refs, NULL));
	clone_bundle();

	cl_git_pass(git_oid_from_string(&id, TAGGED_COMMIT_ID, GIT_OID_SHA1));
	cl_git_pass(git_commit_lookup(&commit, g_clone, &id));
	cl_assert_equal_i(1, git_commit_parentcount(commit));
	git_commit_free(commit);

	cl_assert_equal_sz(2, count_history(g_repo, "refs/tags/e90810b"));
	cl_assert_equal_sz(2, count_history(g_clone, "refs/tags/e90810b"));
}

void test_bundle_create__annotated_tag_is_unpeeled(void)
{
	char *names[] = { "refs/tags/e90810b", "refs/heads/master" };
	git_strarray refs = { names, 2 };
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	git_str contents = GIT_STR_INIT;
	git_remote_head *head;
	git_oid expected;

	cl_git_pass(git_bundle_create(g_repo, BUNDLE_PATH, &refs, NULL));
	cl_git_pass(git_futils_readbuffer(&contents, BUNDLE_PATH));
	cl_git_pass(git_bundle_parser_parse_memory(
		&parser, contents.ptr, contents.size));

	cl_assert_equal_i(2, parser.header.version);
	cl_assert_equal_sz(0, git_array_size(parser.header.prerequisites));
	cl_assert_equal_sz(2, parser.header.refs.length);

	head = git_vector_get(&parser.header.refs, 0);
	cl_assert_equal_s("refs/tags/e90810b", head->name);
	cl_git_pass(git_oid_from_string(&expected, ANNOTATED_TAG_ID, GIT_OID_SHA1));
	cl_assert_equal_oid(&expected, &head->oid);

	head = git_vector_get(&parser.header.refs, 1);
	cl_assert_equal_s("refs/heads/master", head->name);

	git_bundle_parser_dispose(&parser);
	git_str_dispose(&contents);
}

static int write_to_str(const void *buf, size_t len, void *payload)
{
	return git_str_put(payload, buf, len);
}

void test_bundle_create__write_callback(void)
{
	char *names[] = { "refs/heads/master" };
	git_strarray refs = { names, 1 };
	git_str out = GIT_STR_INIT;
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;

	cl_git_pass(git_bundle__write(g_repo, &refs, NULL, write_to_str, &out));
	cl_assert(git__prefixcmp(out.ptr, "# v2 git bundle\n") == 0);
	cl_git_pass(git_bundle_parser_parse_memory(&parser, out.ptr, out.size));
	cl_assert(parser.header.pack_offset < out.size);
	cl_assert(memcmp(out.ptr + parser.header.pack_offset, "PACK", 4) == 0);

	git_bundle_parser_dispose(&parser);
	git_str_dispose(&out);
}

void test_bundle_create__existing_lock(void)
{
	char *names[] = { "refs/heads/master" };
	git_strarray refs = { names, 1 };
	git_str contents = GIT_STR_INIT;

	cl_git_write2file(BUNDLE_PATH ".lock", "locked", 6, O_CREAT | O_TRUNC | O_WRONLY, 0666);

	cl_git_fail_with(GIT_ELOCKED,
		git_bundle_create(g_repo, BUNDLE_PATH, &refs, NULL));

	cl_git_pass(git_futils_readbuffer(&contents, BUNDLE_PATH ".lock"));
	cl_assert_equal_s("locked", contents.ptr);
	cl_assert(!git_fs_path_exists(BUNDLE_PATH));

	git_str_dispose(&contents);
}

/* ---- helpers shared by the tests below ---- */

#define OLD_CONTENTS "previous contents"

static void create_names(
	char **names, size_t count, const git_bundle_create_options *opts)
{
	git_strarray refs;

	refs.strings = names;
	refs.count = count;
	cl_git_pass(git_bundle_create(g_repo, BUNDLE_PATH, &refs, opts));
}

static int create_names_rc(char **names, size_t count)
{
	git_strarray refs;

	refs.strings = names;
	refs.count = count;
	return git_bundle_create(g_repo, BUNDLE_PATH, &refs, NULL);
}

static void write_old_destination(void)
{
	cl_git_write2file(BUNDLE_PATH, OLD_CONTENTS, strlen(OLD_CONTENTS),
		O_CREAT | O_TRUNC | O_WRONLY, 0666);
}

static void assert_old_destination(void)
{
	git_str contents = GIT_STR_INIT;

	cl_git_pass(git_futils_readbuffer(&contents, BUNDLE_PATH));
	cl_assert_equal_s(OLD_CONTENTS, contents.ptr);
	cl_assert(!git_fs_path_exists(BUNDLE_PATH ".lock"));

	git_str_dispose(&contents);
}

static void assert_no_output(void)
{
	cl_assert(!git_fs_path_exists(BUNDLE_PATH));
	cl_assert(!git_fs_path_exists(BUNDLE_PATH ".lock"));
}

static void parse_bundle(git_bundle_parser *parser, git_str *contents)
{
	cl_git_pass(git_futils_readbuffer(contents, BUNDLE_PATH));
	cl_git_pass(git_bundle_parser_parse_memory(
		parser, contents->ptr, contents->size));
}

static git_remote_head *header_ref(git_bundle_parser *parser, size_t i)
{
	cl_assert(i < parser->header.refs.length);
	return git_vector_get(&parser->header.refs, i);
}

static void assert_header_ref(
	git_bundle_parser *parser, size_t i, const char *name, const git_oid *oid)
{
	git_remote_head *head = header_ref(parser, i);

	cl_assert_equal_s(name, head->name);
	if (oid)
		cl_assert_equal_oid(oid, &head->oid);
}

static void ref_oid(git_oid *out, git_repository *repo, const char *name)
{
	git_reference *ref, *resolved;

	cl_git_pass(git_reference_lookup(&ref, repo, name));
	cl_git_pass(git_reference_resolve(&resolved, ref));
	git_oid_cpy(out, git_reference_target(resolved));
	git_reference_free(resolved);
	git_reference_free(ref);
}

static void assert_exists(git_repository *dst, const git_oid *id)
{
	git_odb *odb;

	cl_git_pass(git_repository_odb(&odb, dst));
	cl_assert(git_odb_exists(odb, id));
	git_odb_free(odb);
}

static void assert_tree_available(
	git_repository *src, git_repository *dst, const git_oid *id)
{
	git_tree *tree;
	size_t i;

	assert_exists(dst, id);
	cl_git_pass(git_tree_lookup(&tree, src, id));

	for (i = 0; i < git_tree_entrycount(tree); i++) {
		const git_tree_entry *entry = git_tree_entry_byindex(tree, i);

		switch (git_tree_entry_type(entry)) {
		case GIT_OBJECT_TREE:
			assert_tree_available(src, dst, git_tree_entry_id(entry));
			break;
		case GIT_OBJECT_BLOB:
			assert_exists(dst, git_tree_entry_id(entry));
			break;
		default: /* submodule commits are not part of the bundle */
			break;
		}
	}

	git_tree_free(tree);
}

/* every object reachable from `id` in the source must be in the destination */
static void assert_available(
	git_repository *src, git_repository *dst, const git_oid *id)
{
	git_object *obj;
	git_revwalk *walk;
	git_oid cid;
	git_commit *commit;

	cl_git_pass(git_object_lookup(&obj, src, id, GIT_OBJECT_ANY));

	switch (git_object_type(obj)) {
	case GIT_OBJECT_TAG:
		assert_exists(dst, id);
		assert_available(src, dst, git_tag_target_id((git_tag *)obj));
		break;
	case GIT_OBJECT_COMMIT:
		cl_git_pass(git_revwalk_new(&walk, src));
		cl_git_pass(git_revwalk_push(walk, id));
		while (git_revwalk_next(&cid, walk) == 0) {
			assert_exists(dst, &cid);
			cl_git_pass(git_commit_lookup(&commit, src, &cid));
			assert_tree_available(src, dst, git_commit_tree_id(commit));
			git_commit_free(commit);
		}
		git_revwalk_free(walk);
		break;
	case GIT_OBJECT_TREE:
		assert_tree_available(src, dst, id);
		break;
	default:
		assert_exists(dst, id);
		break;
	}

	git_object_free(obj);
}

/* fetch the whole bundle into a fresh bare repository of any object format */
static void fetch_bundle(git_oid_t oid_type)
{
	git_repository_init_options init = GIT_REPOSITORY_INIT_OPTIONS_INIT;
	git_remote *remote;
	char *specs[] = { "+refs/*:refs/*" };
	git_strarray refspecs = { specs, 1 };

	init.flags = GIT_REPOSITORY_INIT_MKPATH | GIT_REPOSITORY_INIT_BARE;
	init.oid_type = oid_type;
	cl_git_pass(git_repository_init_ext(&g_clone, CLONE_PATH, &init));
	cl_git_pass(git_remote_create(&remote, g_clone, "origin", BUNDLE_PATH));
	cl_git_pass(git_remote_fetch(remote, &refspecs, NULL, NULL));
	git_remote_free(remote);
}

/* every ref advertised by the bundle resolves to the same id and is complete */
static void assert_bundle_matches_source(void)
{
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	git_str contents = GIT_STR_INIT;
	git_remote_head *head;
	git_oid id;
	size_t i;

	parse_bundle(&parser, &contents);

	for (i = 0; i < parser.header.refs.length; i++) {
		head = header_ref(&parser, i);
		ref_oid(&id, g_repo, head->name);
		cl_assert_equal_oid(&id, &head->oid);
		assert_available(g_repo, g_clone, &head->oid);
	}

	git_bundle_parser_dispose(&parser);
	git_str_dispose(&contents);
}

static void make_commit(git_oid *out, const git_oid *parent, const char *msg)
{
	git_signature *sig;
	git_treebuilder *tb;
	git_oid tree_id, blob_id;
	git_tree *tree;
	const git_commit *parents[1];
	git_commit *parent_commit = NULL;

	cl_git_pass(git_signature_now(&sig, "Tester", "tester@example.com"));
	cl_git_pass(git_blob_create_from_buffer(&blob_id, g_repo, msg, strlen(msg)));
	cl_git_pass(git_treebuilder_new(&tb, g_repo, NULL));
	cl_git_pass(git_treebuilder_insert(NULL, tb, "file", &blob_id,
		GIT_FILEMODE_BLOB));
	cl_git_pass(git_treebuilder_write(&tree_id, tb));
	cl_git_pass(git_tree_lookup(&tree, g_repo, &tree_id));

	if (parent) {
		cl_git_pass(git_commit_lookup(&parent_commit, g_repo, parent));
		parents[0] = parent_commit;
	}

	cl_git_pass(git_commit_create(out, g_repo, NULL, sig, sig, NULL, msg,
		tree, parent ? 1 : 0, (const git_commit **)parents));

	git_commit_free(parent_commit);
	git_tree_free(tree);
	git_treebuilder_free(tb);
	git_signature_free(sig);
}

static void make_ref(const char *name, const git_oid *id)
{
	git_reference *ref;

	cl_git_pass(git_reference_create(&ref, g_repo, name, id, 1, NULL));
	git_reference_free(ref);
}

static void make_tag(
	git_oid *out, const char *name, const git_oid *target, git_object_t type)
{
	git_signature *sig;
	git_object *obj;

	cl_git_pass(git_signature_now(&sig, "Tester", "tester@example.com"));
	cl_git_pass(git_object_lookup(&obj, g_repo, target, type));
	cl_git_pass(git_tag_create(out, g_repo, name, obj, sig, "tag message\n", 1));
	git_object_free(obj);
	git_signature_free(sig);
}

/* ---- histories ---- */

void test_bundle_create__multiple_branches_shared_history(void)
{
	char *names[] = { "refs/heads/master", "refs/heads/br2",
		"refs/heads/test", "refs/heads/packed-test" };

	create_names(names, 4, NULL);
	clone_bundle();

	assert_bundle_matches_source();
	cl_assert_equal_sz(count_history(g_repo, "refs/heads/br2"),
		count_history(g_clone, "refs/remotes/origin/br2"));
	cl_assert_equal_sz(count_history(g_repo, "refs/heads/packed-test"),
		count_history(g_clone, "refs/remotes/origin/packed-test"));
}

void test_bundle_create__disconnected_histories(void)
{
	char *names[] = { "refs/heads/master", "refs/heads/orphan" };
	git_oid orphan, orphan_child, master;

	make_commit(&orphan, NULL, "orphan root");
	make_commit(&orphan_child, &orphan, "orphan child");
	make_ref("refs/heads/orphan", &orphan_child);

	create_names(names, 2, NULL);
	fetch_bundle(GIT_OID_SHA1);

	assert_bundle_matches_source();
	ref_oid(&master, g_clone, "refs/heads/master");
	cl_assert_equal_sz(2, count_history(g_clone, "refs/heads/orphan"));
	cl_assert_equal_sz(count_history(g_repo, "refs/heads/master"),
		count_history(g_clone, "refs/heads/master"));
}

/* ---- tags ---- */

void test_bundle_create__lightweight_tag(void)
{
	char *names[] = { "refs/tags/light" };
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	git_str contents = GIT_STR_INIT;
	git_oid master;

	ref_oid(&master, g_repo, "refs/heads/master");
	make_ref("refs/tags/light", &master);

	create_names(names, 1, NULL);
	parse_bundle(&parser, &contents);
	cl_assert_equal_sz(1, parser.header.refs.length);
	assert_header_ref(&parser, 0, "refs/tags/light", &master);

	fetch_bundle(GIT_OID_SHA1);
	assert_bundle_matches_source();

	git_bundle_parser_dispose(&parser);
	git_str_dispose(&contents);
}

void test_bundle_create__tag_chain(void)
{
	char *names[] = { "refs/tags/outer" };
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	git_str contents = GIT_STR_INIT;
	git_oid inner, middle, outer, commit;

	cl_git_pass(git_oid_from_string(&inner, ANNOTATED_TAG_ID, GIT_OID_SHA1));
	cl_git_pass(git_oid_from_string(&commit, TAGGED_COMMIT_ID, GIT_OID_SHA1));
	make_tag(&middle, "middle", &inner, GIT_OBJECT_TAG);
	make_tag(&outer, "outer", &middle, GIT_OBJECT_TAG);

	create_names(names, 1, NULL);
	parse_bundle(&parser, &contents);
	assert_header_ref(&parser, 0, "refs/tags/outer", &outer);

	fetch_bundle(GIT_OID_SHA1);
	assert_exists(g_clone, &outer);
	assert_exists(g_clone, &middle);
	assert_exists(g_clone, &inner);
	assert_exists(g_clone, &commit);
	assert_bundle_matches_source();

	git_bundle_parser_dispose(&parser);
	git_str_dispose(&contents);
}

void test_bundle_create__tree_and_blob_tags(void)
{
	char *names[] = { "refs/tags/tree_light", "refs/tags/blob_light",
		"refs/tags/tree_annotated" };
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	git_str contents = GIT_STR_INIT;
	git_oid tree, blob, annotated;
	git_commit *commit;
	git_oid master;

	ref_oid(&master, g_repo, "refs/heads/master");
	cl_git_pass(git_commit_lookup(&commit, g_repo, &master));
	git_oid_cpy(&tree, git_commit_tree_id(commit));
	git_commit_free(commit);
	cl_git_pass(git_blob_create_from_buffer(&blob, g_repo, "blob only\n", 10));
	make_tag(&annotated, "tree_annotated", &tree, GIT_OBJECT_TREE);

	make_ref("refs/tags/tree_light", &tree);
	make_ref("refs/tags/blob_light", &blob);

	create_names(names, 3, NULL);
	parse_bundle(&parser, &contents);
	assert_header_ref(&parser, 0, "refs/tags/tree_light", &tree);
	assert_header_ref(&parser, 1, "refs/tags/blob_light", &blob);
	assert_header_ref(&parser, 2, "refs/tags/tree_annotated", &annotated);

	fetch_bundle(GIT_OID_SHA1);
	assert_exists(g_clone, &annotated);
	assert_exists(g_clone, &blob);
	assert_bundle_matches_source();

	git_bundle_parser_dispose(&parser);
	git_str_dispose(&contents);
}

/* ---- HEAD and names ---- */

void test_bundle_create__symbolic_head(void)
{
	char *names[] = { "HEAD" };
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	git_str contents = GIT_STR_INIT;
	git_oid master;

	create_names(names, 1, NULL);
	parse_bundle(&parser, &contents);

	ref_oid(&master, g_repo, "refs/heads/master");
	cl_assert_equal_sz(1, parser.header.refs.length);
	assert_header_ref(&parser, 0, "HEAD", &master);

	git_bundle_parser_dispose(&parser);
	git_str_dispose(&contents);
}

void test_bundle_create__detached_head(void)
{
	char *names[] = { "HEAD", "refs/heads/master" };
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	git_str contents = GIT_STR_INIT;
	git_oid br2, master;

	ref_oid(&br2, g_repo, "refs/heads/br2");
	ref_oid(&master, g_repo, "refs/heads/master");
	cl_assert(!git_oid_equal(&br2, &master));
	cl_git_pass(git_repository_set_head_detached(g_repo, &br2));

	create_names(names, 2, NULL);
	parse_bundle(&parser, &contents);
	cl_assert_equal_sz(2, parser.header.refs.length);
	assert_header_ref(&parser, 0, "HEAD", &br2);
	assert_header_ref(&parser, 1, "refs/heads/master", &master);

	git_bundle_parser_dispose(&parser);
	git_str_dispose(&contents);
}

void test_bundle_create__omitted_head_not_advertised(void)
{
	char *names[] = { "refs/heads/master" };
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	git_str contents = GIT_STR_INIT;
	size_t i;

	create_names(names, 1, NULL);
	parse_bundle(&parser, &contents);

	cl_assert_equal_sz(1, parser.header.refs.length);
	for (i = 0; i < parser.header.refs.length; i++)
		cl_assert(strcmp(header_ref(&parser, i)->name, "HEAD") != 0);

	git_bundle_parser_dispose(&parser);
	git_str_dispose(&contents);
}

void test_bundle_create__duplicates_keep_first_position(void)
{
	char *names[] = { "refs/heads/master", "refs/heads/br2",
		"refs/heads/master", "refs/heads/br2" };
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	git_str contents = GIT_STR_INIT;

	create_names(names, 4, NULL);
	parse_bundle(&parser, &contents);

	cl_assert_equal_sz(2, parser.header.refs.length);
	assert_header_ref(&parser, 0, "refs/heads/master", NULL);
	assert_header_ref(&parser, 1, "refs/heads/br2", NULL);

	git_bundle_parser_dispose(&parser);
	git_str_dispose(&contents);
}

void test_bundle_create__invalid_names(void)
{
	char *shorthand[] = { "master" };
	char *bad[] = { "refs/heads/..bad" };
	char *mixed[] = { "refs/heads/master", "master" };

	cl_git_fail_with(GIT_EINVALIDSPEC, create_names_rc(shorthand, 1));
	assert_no_output();
	cl_git_fail_with(GIT_EINVALIDSPEC, create_names_rc(bad, 1));
	assert_no_output();
	cl_git_fail_with(GIT_EINVALIDSPEC, create_names_rc(mixed, 2));
	assert_no_output();
}

void test_bundle_create__missing_ref(void)
{
	char *names[] = { "refs/heads/master", "refs/heads/does-not-exist" };

	cl_git_fail_with(GIT_ENOTFOUND, create_names_rc(names, 2));
	assert_no_output();
}

void test_bundle_create__unborn_head(void)
{
	char *names[] = { "HEAD" };

	cl_git_pass(git_repository_set_head(g_repo, "refs/heads/unborn"));
	cl_git_fail_with(GIT_ENOTFOUND, create_names_rc(names, 1));
	assert_no_output();
}

void test_bundle_create__empty_selection(void)
{
	git_strarray empty = { NULL, 0 };

	cl_git_fail_with(-1, git_bundle_create(g_repo, BUNDLE_PATH, &empty, NULL));
	assert_no_output();
	cl_git_fail_with(-1, git_bundle_create(g_repo, BUNDLE_PATH, NULL, NULL));
	assert_no_output();
}

/* ---- header ordering ---- */

void test_bundle_create__header_follows_caller_order(void)
{
	char *forward[] = { "refs/heads/br2", "HEAD", "refs/tags/test",
		"refs/heads/master" };
	char *reverse[] = { "refs/heads/master", "refs/tags/test", "HEAD",
		"refs/heads/br2" };
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	git_str contents = GIT_STR_INIT;
	size_t i;

	create_names(forward, 4, NULL);
	parse_bundle(&parser, &contents);
	cl_assert_equal_sz(4, parser.header.refs.length);
	for (i = 0; i < 4; i++)
		assert_header_ref(&parser, i, forward[i], NULL);
	git_bundle_parser_dispose(&parser);
	git_str_dispose(&contents);

	create_names(reverse, 4, NULL);
	parse_bundle(&parser, &contents);
	cl_assert_equal_sz(4, parser.header.refs.length);
	for (i = 0; i < 4; i++)
		assert_header_ref(&parser, i, reverse[i], NULL);

	git_bundle_parser_dispose(&parser);
	git_str_dispose(&contents);
}

/* ---- object availability ---- */

void test_bundle_create__complete_objects_in_fresh_destination(void)
{
	char *names[] = { "HEAD", "refs/heads/master", "refs/heads/br2",
		"refs/tags/e90810b", "refs/tags/wrapped_tag" };

	create_names(names, 5, NULL);
	fetch_bundle(GIT_OID_SHA1);
	assert_bundle_matches_source();
}

/* ---- failures ---- */

void test_bundle_create__shallow_source(void)
{
	char *names[] = { "refs/heads/master" };

	cl_git_mkfile("testrepo.git/shallow", TAGGED_COMMIT_ID "\n");
	cl_assert_equal_i(1, git_repository_is_shallow(g_repo));

	cl_git_fail_with(GIT_ENOTSUPPORTED, create_names_rc(names, 1));
	assert_no_output();

	write_old_destination();
	cl_git_fail_with(GIT_ENOTSUPPORTED, create_names_rc(names, 1));
	assert_old_destination();
}

void test_bundle_create__missing_object(void)
{
	char *names[] = { "refs/heads/master" };
	git_oid master;
	git_commit *commit;
	git_tree *tree;
	const git_tree_entry *entry;
	git_str path = GIT_STR_INIT;
	char hex[GIT_OID_MAX_HEXSIZE + 1];
	size_t i;
	int removed = 0;

	ref_oid(&master, g_repo, "refs/heads/master");
	cl_git_pass(git_commit_lookup(&commit, g_repo, &master));
	cl_git_pass(git_commit_tree(&tree, commit));

	/* remove the first loose blob in the head commit's tree */
	for (i = 0; i < git_tree_entrycount(tree) && !removed; i++) {
		entry = git_tree_entry_byindex(tree, i);
		if (git_tree_entry_type(entry) != GIT_OBJECT_BLOB)
			continue;

		git_oid_tostr(hex, sizeof(hex), git_tree_entry_id(entry));
		cl_git_pass(git_str_printf(&path, "testrepo.git/objects/%.2s/%s",
			hex, hex + 2));
		if (git_fs_path_isfile(path.ptr)) {
			cl_must_pass(p_unlink(path.ptr));
			removed = 1;
		}
		git_str_clear(&path);
	}
	cl_assert(removed);

	git_str_dispose(&path);
	git_tree_free(tree);
	git_commit_free(commit);

	write_old_destination();
	cl_git_fail(create_names_rc(names, 1));
	assert_old_destination();
}

static int cancel_cb(int stage, uint32_t current, uint32_t total, void *payload)
{
	int *calls = payload;

	GIT_UNUSED(stage);
	GIT_UNUSED(current);
	GIT_UNUSED(total);

	(*calls)++;
	return -42;
}

void test_bundle_create__cancellation(void)
{
	char *names[] = { "refs/heads/master" };
	git_strarray refs = { names, 1 };
	git_bundle_create_options opts = GIT_BUNDLE_CREATE_OPTIONS_INIT;
	int calls = 0;

	opts.progress_cb = cancel_cb;
	opts.progress_cb_payload = &calls;

	write_old_destination();
	cl_git_fail_with(-42, git_bundle_create(g_repo, BUNDLE_PATH, &refs, &opts));
	cl_assert(calls > 0);
	assert_old_destination();

	cl_must_pass(p_unlink(BUNDLE_PATH));
	calls = 0;
	cl_git_fail_with(-42, git_bundle_create(g_repo, BUNDLE_PATH, &refs, &opts));
	cl_assert(calls > 0);
	assert_no_output();
}

typedef struct {
	size_t calls;
	size_t fail_at;
} failing_writer;

static int write_failing(const void *buf, size_t len, void *payload)
{
	failing_writer *w = payload;

	GIT_UNUSED(buf);
	GIT_UNUSED(len);

	if (++w->calls == w->fail_at)
		return -7;

	return 0;
}

void test_bundle_create__write_callback_failure(void)
{
	char *names[] = { "refs/heads/master" };
	git_strarray refs = { names, 1 };
	failing_writer w = { 0, 0 };
	size_t total, n;

	/* learn how many writes a successful run makes */
	cl_git_pass(git_bundle__write(g_repo, &refs, NULL, write_failing, &w));
	total = w.calls;
	cl_assert(total >= 2);

	for (n = 1; n <= total; n++) {
		w.calls = 0;
		w.fail_at = n;
		cl_git_fail_with(-7,
			git_bundle__write(g_repo, &refs, NULL, write_failing, &w));
		cl_assert_equal_sz(n, w.calls);
	}
}

void test_bundle_create__missing_parent_directory(void)
{
	char *names[] = { "refs/heads/master" };
	git_strarray refs = { names, 1 };

	cl_git_fail(git_bundle_create(g_repo, "no_such_dir/out.bundle", &refs, NULL));
	cl_assert(!git_fs_path_exists("no_such_dir"));
}

void test_bundle_create__overwrite_existing_destination(void)
{
	char *first[] = { "refs/heads/br2" };
	char *second[] = { "refs/heads/master" };
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	git_str contents = GIT_STR_INIT;
	git_oid master;

	create_names(first, 1, NULL);
	create_names(second, 1, NULL);

	parse_bundle(&parser, &contents);
	ref_oid(&master, g_repo, "refs/heads/master");
	cl_assert_equal_sz(1, parser.header.refs.length);
	assert_header_ref(&parser, 0, "refs/heads/master", &master);
	cl_assert(!git_fs_path_exists(BUNDLE_PATH ".lock"));

	clone_bundle();
	assert_bundle_matches_source();

	git_bundle_parser_dispose(&parser);
	git_str_dispose(&contents);
}

/* ---- SHA-256 ---- */

void test_bundle_create__sha256(void)
{
	char *names[] = { "HEAD", "refs/heads/master", "refs/tags/e90810b" };
	git_bundle_parser parser = GIT_BUNDLE_PARSER_INIT;
	git_str contents = GIT_STR_INIT;
	git_oid master;

	cl_git_sandbox_cleanup();
	g_repo = cl_git_sandbox_init("testrepo_256.git");
	cl_assert_equal_i(GIT_OID_SHA256, git_repository_oid_type(g_repo));

	create_names(names, 3, NULL);
	parse_bundle(&parser, &contents);

	cl_assert(git__prefixcmp(contents.ptr,
		"# v3 git bundle\n@object-format=sha256\n") == 0);
	cl_assert_equal_i(3, parser.header.version);
	cl_assert_equal_i(GIT_OID_SHA256, parser.header.oid_type);
	cl_assert_equal_sz(3, parser.header.refs.length);

	ref_oid(&master, g_repo, "refs/heads/master");
	assert_header_ref(&parser, 0, "HEAD", &master);
	assert_header_ref(&parser, 1, "refs/heads/master", &master);
	assert_header_ref(&parser, 2, "refs/tags/e90810b", NULL);

	fetch_bundle(GIT_OID_SHA256);
	cl_assert_equal_i(GIT_OID_SHA256, git_repository_oid_type(g_clone));
	assert_bundle_matches_source();

	git_bundle_parser_dispose(&parser);
	git_str_dispose(&contents);
}
