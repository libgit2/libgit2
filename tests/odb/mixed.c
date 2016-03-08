#include "clar_libgit2.h"
#include "odb.h"

static git_odb *_odb;

void test_odb_mixed__initialize(void)
{
	cl_git_pass(git_odb_open(&_odb, cl_fixture("duplicate.git/objects")));
}

void test_odb_mixed__cleanup(void)
{
	git_odb_free(_odb);
	_odb = NULL;
}

void test_odb_mixed__dup_oid(void) {
	const char hex[] = "ce013625030ba8dba906f756967f9e9ca394464a";
	const char short_hex[] = "ce01362";
	git_oid oid;
	git_odb_object *obj;

	cl_git_pass(git_oid_fromstr(&oid, hex));
	cl_git_pass(git_odb_read_prefix(&obj, _odb, &oid, GIT_OID_HEXSZ));
	git_odb_object_free(obj);

	cl_git_pass(git_odb_exists_prefix(NULL, _odb, &oid, GIT_OID_HEXSZ));

	cl_git_pass(git_oid_fromstrn(&oid, short_hex, sizeof(short_hex) - 1));
	cl_git_pass(git_odb_read_prefix(&obj, _odb, &oid, sizeof(short_hex) - 1));
	git_odb_object_free(obj);

	cl_git_pass(git_odb_exists_prefix(NULL, _odb, &oid, sizeof(short_hex) - 1));
}

/* some known sha collisions of file content:
 *   'aabqhq' and 'aaazvc' with prefix 'dea509d0' (+ '9' and + 'b')
 *   'aaeufo' and 'aaaohs' with prefix '81b5bff5' (+ 'f' and + 'b')
 *   'aafewy' and 'aaepta' with prefix '739e3c4c'
 *   'aahsyn' and 'aadrjg' with prefix '0ddeaded' (+ '9' and + 'e')
 */

void test_odb_mixed__dup_oid_prefix_0(void) {
	char hex[10];
	git_oid oid, found;
	git_odb_object *obj;

	/* ambiguous in the same pack file */

	strncpy(hex, "dea509d0", sizeof(hex));
	cl_git_pass(git_oid_fromstrn(&oid, hex, strlen(hex)));
	cl_assert_equal_i(
		GIT_EAMBIGUOUS, git_odb_read_prefix(&obj, _odb, &oid, strlen(hex)));
	cl_assert_equal_i(
		GIT_EAMBIGUOUS, git_odb_exists_prefix(&found, _odb, &oid, strlen(hex)));

	strncpy(hex, "dea509d09", sizeof(hex));
	cl_git_pass(git_oid_fromstrn(&oid, hex, strlen(hex)));
	cl_git_pass(git_odb_read_prefix(&obj, _odb, &oid, strlen(hex)));
	cl_git_pass(git_odb_exists_prefix(&found, _odb, &oid, strlen(hex)));
	cl_assert_equal_oid(&found, git_odb_object_id(obj));
	git_odb_object_free(obj);

	strncpy(hex, "dea509d0b", sizeof(hex));
	cl_git_pass(git_oid_fromstrn(&oid, hex, strlen(hex)));
	cl_git_pass(git_odb_read_prefix(&obj, _odb, &oid, strlen(hex)));
	git_odb_object_free(obj);

	/* ambiguous in different pack files */

	strncpy(hex, "81b5bff5", sizeof(hex));
	cl_git_pass(git_oid_fromstrn(&oid, hex, strlen(hex)));
	cl_assert_equal_i(
		GIT_EAMBIGUOUS, git_odb_read_prefix(&obj, _odb, &oid, strlen(hex)));
	cl_assert_equal_i(
		GIT_EAMBIGUOUS, git_odb_exists_prefix(&found, _odb, &oid, strlen(hex)));

	strncpy(hex, "81b5bff5b", sizeof(hex));
	cl_git_pass(git_oid_fromstrn(&oid, hex, strlen(hex)));
	cl_git_pass(git_odb_read_prefix(&obj, _odb, &oid, strlen(hex)));
	cl_git_pass(git_odb_exists_prefix(&found, _odb, &oid, strlen(hex)));
	cl_assert_equal_oid(&found, git_odb_object_id(obj));
	git_odb_object_free(obj);

	strncpy(hex, "81b5bff5f", sizeof(hex));
	cl_git_pass(git_oid_fromstrn(&oid, hex, strlen(hex)));
	cl_git_pass(git_odb_read_prefix(&obj, _odb, &oid, strlen(hex)));
	git_odb_object_free(obj);

	/* ambiguous in pack file and loose */

	strncpy(hex, "0ddeaded", sizeof(hex));
	cl_git_pass(git_oid_fromstrn(&oid, hex, strlen(hex)));
	cl_assert_equal_i(
		GIT_EAMBIGUOUS, git_odb_read_prefix(&obj, _odb, &oid, strlen(hex)));
	cl_assert_equal_i(
		GIT_EAMBIGUOUS, git_odb_exists_prefix(&found, _odb, &oid, strlen(hex)));

	strncpy(hex, "0ddeaded9", sizeof(hex));
	cl_git_pass(git_oid_fromstrn(&oid, hex, strlen(hex)));
	cl_git_pass(git_odb_read_prefix(&obj, _odb, &oid, strlen(hex)));
	cl_git_pass(git_odb_exists_prefix(&found, _odb, &oid, strlen(hex)));
	cl_assert_equal_oid(&found, git_odb_object_id(obj));
	git_odb_object_free(obj);

	strncpy(hex, "0ddeadede", sizeof(hex));
	cl_git_pass(git_oid_fromstrn(&oid, hex, strlen(hex)));
	cl_git_pass(git_odb_read_prefix(&obj, _odb, &oid, strlen(hex)));
	git_odb_object_free(obj);
}

struct expand_id_test_data {
	char *lookup_id;
	char *expected_id;
	git_otype expected_type;
};

struct expand_id_test_data expand_id_test_data[] = {
	/* some prefixes and their expected values */
	{ "dea509d0",  NULL, GIT_OBJ_ANY },
	{ "00000000",  NULL, GIT_OBJ_ANY },
	{ "dea509d0",  NULL, GIT_OBJ_ANY },
	{ "dea509d09", "dea509d097ce692e167dfc6a48a7a280cc5e877e", GIT_OBJ_BLOB },
	{ "dea509d0b", "dea509d0b3cb8ee0650f6ca210bc83f4678851ba", GIT_OBJ_BLOB },
	{ "ce0136250", "ce013625030ba8dba906f756967f9e9ca394464a", GIT_OBJ_BLOB },
	{ "0ddeaded",  NULL, GIT_OBJ_ANY },
	{ "4d5979b",   "4d5979b468252190cb572ae758aca36928e8a91e", GIT_OBJ_TREE },
	{ "0ddeaded",  NULL, GIT_OBJ_ANY },
	{ "0ddeadede", "0ddeadede9e6d6ccddce0ee1e5749eed0485e5ea", GIT_OBJ_BLOB },
	{ "0ddeaded9", "0ddeaded9502971eefe1e41e34d0e536853ae20f", GIT_OBJ_BLOB },
	{ "f00b4e",    NULL, GIT_OBJ_ANY },

	/* some full-length object ids */
	{ "0000000000000000000000000000000000000000", NULL, GIT_OBJ_ANY },
	{
	  "dea509d097ce692e167dfc6a48a7a280cc5e877e",
	  "dea509d097ce692e167dfc6a48a7a280cc5e877e",
	  GIT_OBJ_BLOB
	},
	{ "f00f00f00f00f00f00f00f00f00f00f00f00f00f", NULL, GIT_OBJ_ANY },
	{
	  "4d5979b468252190cb572ae758aca36928e8a91e",
	  "4d5979b468252190cb572ae758aca36928e8a91e",
	  GIT_OBJ_TREE
	},
};

static void setup_prefix_query(
	git_oid **out_ids,
	size_t **out_lengths,
	git_otype **out_types,
	size_t *out_num)
{
	git_oid *ids;
	git_otype *types;
	size_t num, *lengths, i;

	num = ARRAY_SIZE(expand_id_test_data);

	cl_assert((ids = git__calloc(num, sizeof(git_oid))));
	cl_assert((lengths = git__calloc(num, sizeof(size_t))));
	cl_assert((types = git__calloc(num, sizeof(git_otype))));

	for (i = 0; i < num; i++) {
		lengths[i] = strlen(expand_id_test_data[i].lookup_id);
		git_oid_fromstrn(&ids[i], expand_id_test_data[i].lookup_id, lengths[i]);
		types[i] = expand_id_test_data[i].expected_type;
	}

	*out_ids = ids;
	*out_lengths = lengths;
	*out_types = types;
	*out_num = num;
}

static void assert_found_objects(
	git_oid *ids, size_t *lengths, git_otype *types)
{
	size_t num, i;

	num = ARRAY_SIZE(expand_id_test_data);

	for (i = 0; i < num; i++) {
		git_oid expected_id = {{0}};
		size_t expected_len = 0;
		git_otype expected_type = 0;

		if (expand_id_test_data[i].expected_id) {
			git_oid_fromstr(&expected_id, expand_id_test_data[i].expected_id);
			expected_len = GIT_OID_HEXSZ;
			expected_type = expand_id_test_data[i].expected_type;
		}

		cl_assert_equal_i(expected_len, lengths[i]);
		cl_assert_equal_oid(&expected_id, &ids[i]);

		if (types)
			cl_assert_equal_i(expected_type, types[i]);
	}
}

static void assert_notfound_objects(
	git_oid *ids, size_t *lengths, git_otype *types)
{
	git_oid expected_id = {{0}};
	size_t num, i;

	num = ARRAY_SIZE(expand_id_test_data);

	for (i = 0; i < num; i++) {
		cl_assert_equal_i(0, lengths[i]);
		cl_assert_equal_oid(&expected_id, &ids[i]);

		if (types)
			cl_assert_equal_i(0, types[i]);
	}
}

void test_odb_mixed__expand_ids(void)
{
	git_oid *ids;
	size_t i, num, *lengths;
	git_otype *types;

	/* test looking for the actual (correct) types */

	setup_prefix_query(&ids, &lengths, &types, &num);
	cl_git_pass(git_odb_expand_ids(_odb, ids, lengths, types, num));
	assert_found_objects(ids, lengths, types);
	git__free(ids); git__free(lengths); git__free(types);

	/* test looking for no specified types (types array == NULL) */

	setup_prefix_query(&ids, &lengths, &types, &num);
	cl_git_pass(git_odb_expand_ids(_odb, ids, lengths, NULL, num));
	assert_found_objects(ids, lengths, NULL);
	git__free(ids); git__free(lengths); git__free(types);

	/* test looking for an explicit GIT_OBJ_ANY */

	setup_prefix_query(&ids, &lengths, &types, &num);

	for (i = 0; i < num; i++)
		types[i] = GIT_OBJ_ANY;

	cl_git_pass(git_odb_expand_ids(_odb, ids, lengths, types, num));
	assert_found_objects(ids, lengths, types);
	git__free(ids); git__free(lengths); git__free(types);

	/* test looking for the completely wrong type */

	setup_prefix_query(&ids, &lengths, &types, &num);

	for (i = 0; i < num; i++)
		types[i] = (types[i] == GIT_OBJ_BLOB) ? GIT_OBJ_TREE : GIT_OBJ_BLOB;

	cl_git_pass(git_odb_expand_ids(_odb, ids, lengths, types, num));
	assert_notfound_objects(ids, lengths, types);
	git__free(ids); git__free(lengths); git__free(types);
}

