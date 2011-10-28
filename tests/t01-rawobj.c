/*
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * In addition to the permissions in the GNU General Public License,
 * the authors give you unlimited permission to link the compiled
 * version of this file into combinations with other programs,
 * and to distribute those combinations without any restriction
 * coming from the use of this file.  (The General Public License
 * restrictions do apply in other respects; for example, they cover
 * modification of the file, and distribution when not linked into
 * a combined executable.)
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#include "test_lib.h"

#include "odb.h"
#include "hash.h"

#include "t01-data.h"

static int hash_object(git_oid *oid, git_rawobj *obj)
{
	return git_odb_hash(oid, obj->data, obj->len, obj->type);
}

BEGIN_TEST(oid0, "validate size of oid objects")
	git_oid out;
	must_be_true(20 == GIT_OID_RAWSZ);
	must_be_true(40 == GIT_OID_HEXSZ);
	must_be_true(sizeof(out) == GIT_OID_RAWSZ);
	must_be_true(sizeof(out.id) == GIT_OID_RAWSZ);
END_TEST

BEGIN_TEST(oid1, "fail when parsing an empty string as oid")
	git_oid out;
	must_fail(git_oid_fromstr(&out, ""));
END_TEST

BEGIN_TEST(oid2, "fail when parsing an invalid string as oid")
	git_oid out;
	must_fail(git_oid_fromstr(&out, "moo"));
END_TEST

static int from_hex(unsigned int i)
{
	if (i >= '0' && i <= '9')
		return i - '0';
	if (i >= 'a' && i <= 'f')
		return 10 + (i - 'a');
	if (i >= 'A' && i <= 'F')
		return 10 + (i - 'A');
	return -1;
}

BEGIN_TEST(oid3, "find all invalid characters when parsing an oid")
	git_oid out;
	unsigned char exp[] = {
		0x16, 0xa6, 0x77, 0x70, 0xb7,
		0xd8, 0xd7, 0x23, 0x17, 0xc4,
		0xb7, 0x75, 0x21, 0x3c, 0x23,
		0xa8, 0xbd, 0x74, 0xf5, 0xe0,
	};
	char in[41] = "16a67770b7d8d72317c4b775213c23a8bd74f5e0";
	unsigned int i;

	for (i = 0; i < 256; i++) {
		in[38] = (char)i;

		if (from_hex(i) >= 0) {
			exp[19] = (unsigned char)(from_hex(i) << 4);
			must_pass(git_oid_fromstr(&out, in));
			must_be_true(memcmp(out.id, exp, sizeof(out.id)) == 0);
		} else {
			must_fail(git_oid_fromstr(&out, in));
		}
	}
END_TEST

BEGIN_TEST(oid4, "fail when parsing an invalid oid string")
	git_oid out;
	must_fail(git_oid_fromstr(&out, "16a67770b7d8d72317c4b775213c23a8bd74f5ez"));
END_TEST

BEGIN_TEST(oid5, "succeed when parsing a valid oid string")
	git_oid out;
	unsigned char exp[] = {
		0x16, 0xa6, 0x77, 0x70, 0xb7,
		0xd8, 0xd7, 0x23, 0x17, 0xc4,
		0xb7, 0x75, 0x21, 0x3c, 0x23,
		0xa8, 0xbd, 0x74, 0xf5, 0xe0,
	};

	must_pass(git_oid_fromstr(&out, "16a67770b7d8d72317c4b775213c23a8bd74f5e0"));
	must_pass(memcmp(out.id, exp, sizeof(out.id)));

	must_pass(git_oid_fromstr(&out, "16A67770B7D8D72317C4b775213C23A8BD74F5E0"));
	must_pass(memcmp(out.id, exp, sizeof(out.id)));
END_TEST

BEGIN_TEST(oid6, "build a valid oid from raw bytes")
	git_oid out;
	unsigned char exp[] = {
		0x16, 0xa6, 0x77, 0x70, 0xb7,
		0xd8, 0xd7, 0x23, 0x17, 0xc4,
		0xb7, 0x75, 0x21, 0x3c, 0x23,
		0xa8, 0xbd, 0x74, 0xf5, 0xe0,
	};

	git_oid_fromraw(&out, exp);
	must_pass(memcmp(out.id, exp, sizeof(out.id)));
END_TEST

BEGIN_TEST(oid7, "properly copy an oid to another")
	git_oid a, b;
	unsigned char exp[] = {
		0x16, 0xa6, 0x77, 0x70, 0xb7,
		0xd8, 0xd7, 0x23, 0x17, 0xc4,
		0xb7, 0x75, 0x21, 0x3c, 0x23,
		0xa8, 0xbd, 0x74, 0xf5, 0xe0,
	};

	memset(&b, 0, sizeof(b));
	git_oid_fromraw(&a, exp);
	git_oid_cpy(&b, &a);
	must_pass(memcmp(a.id, exp, sizeof(a.id)));
END_TEST

BEGIN_TEST(oid8, "compare two oids (lesser than)")
	git_oid a, b;
	unsigned char a_in[] = {
		0x16, 0xa6, 0x77, 0x70, 0xb7,
		0xd8, 0xd7, 0x23, 0x17, 0xc4,
		0xb7, 0x75, 0x21, 0x3c, 0x23,
		0xa8, 0xbd, 0x74, 0xf5, 0xe0,
	};
	unsigned char b_in[] = {
		0x16, 0xa6, 0x77, 0x70, 0xb7,
		0xd8, 0xd7, 0x23, 0x17, 0xc4,
		0xb7, 0x75, 0x21, 0x3c, 0x23,
		0xa8, 0xbd, 0x74, 0xf5, 0xf0,
	};

	git_oid_fromraw(&a, a_in);
	git_oid_fromraw(&b, b_in);
	must_be_true(git_oid_cmp(&a, &b) < 0);
END_TEST

BEGIN_TEST(oid9, "compare two oids (equal)")
	git_oid a, b;
	unsigned char a_in[] = {
		0x16, 0xa6, 0x77, 0x70, 0xb7,
		0xd8, 0xd7, 0x23, 0x17, 0xc4,
		0xb7, 0x75, 0x21, 0x3c, 0x23,
		0xa8, 0xbd, 0x74, 0xf5, 0xe0,
	};

	git_oid_fromraw(&a, a_in);
	git_oid_fromraw(&b, a_in);
	must_be_true(git_oid_cmp(&a, &b) == 0);
END_TEST

BEGIN_TEST(oid10, "compare two oids (greater than)")
	git_oid a, b;
	unsigned char a_in[] = {
		0x16, 0xa6, 0x77, 0x70, 0xb7,
		0xd8, 0xd7, 0x23, 0x17, 0xc4,
		0xb7, 0x75, 0x21, 0x3c, 0x23,
		0xa8, 0xbd, 0x74, 0xf5, 0xe0,
	};
	unsigned char b_in[] = {
		0x16, 0xa6, 0x77, 0x70, 0xb7,
		0xd8, 0xd7, 0x23, 0x17, 0xc4,
		0xb7, 0x75, 0x21, 0x3c, 0x23,
		0xa8, 0xbd, 0x74, 0xf5, 0xd0,
	};

	git_oid_fromraw(&a, a_in);
	git_oid_fromraw(&b, b_in);
	must_be_true(git_oid_cmp(&a, &b) > 0);
END_TEST

BEGIN_TEST(oid11, "compare formated oids")
	const char *exp = "16a0123456789abcdef4b775213c23a8bd74f5e0";
	git_oid in;
	char out[GIT_OID_HEXSZ + 1];

	must_pass(git_oid_fromstr(&in, exp));

	/* Format doesn't touch the last byte */
	out[GIT_OID_HEXSZ] = 'Z';
	git_oid_fmt(out, &in);
	must_be_true(out[GIT_OID_HEXSZ] == 'Z');

	/* Format produced the right result */
	out[GIT_OID_HEXSZ] = '\0';
	must_be_true(strcmp(exp, out) == 0);
END_TEST

BEGIN_TEST(oid12, "compare oids (allocate + format)")
	const char *exp = "16a0123456789abcdef4b775213c23a8bd74f5e0";
	git_oid in;
	char *out;

	must_pass(git_oid_fromstr(&in, exp));

	out = git_oid_allocfmt(&in);
	must_be_true(out);
	must_be_true(strcmp(exp, out) == 0);
	git__free(out);
END_TEST

BEGIN_TEST(oid13, "compare oids (path format)")
	const char *exp1 = "16a0123456789abcdef4b775213c23a8bd74f5e0";
	const char *exp2 = "16/a0123456789abcdef4b775213c23a8bd74f5e0";
	git_oid in;
	char out[GIT_OID_HEXSZ + 2];

	must_pass(git_oid_fromstr(&in, exp1));

	/* Format doesn't touch the last byte */
	out[GIT_OID_HEXSZ + 1] = 'Z';
	git_oid_pathfmt(out, &in);
	must_be_true(out[GIT_OID_HEXSZ + 1] == 'Z');

	/* Format produced the right result */
	out[GIT_OID_HEXSZ + 1] = '\0';
	must_be_true(strcmp(exp2, out) == 0);
END_TEST

BEGIN_TEST(oid14, "convert raw oid to string")
	const char *exp = "16a0123456789abcdef4b775213c23a8bd74f5e0";
	git_oid in;
	char out[GIT_OID_HEXSZ + 1];
	char *str;
	int i;

	must_pass(git_oid_fromstr(&in, exp));

	/* NULL buffer pointer, returns static empty string */
	str = git_oid_to_string(NULL, sizeof(out), &in);
	must_be_true(str && *str == '\0' && str != out);

	/* zero buffer size, returns static empty string */
	str = git_oid_to_string(out, 0, &in);
	must_be_true(str && *str == '\0' && str != out);

	/* NULL oid pointer, returns static empty string */
	str = git_oid_to_string(out, sizeof(out), NULL);
	must_be_true(str && *str == '\0' && str != out);

	/* n == 1, returns out as an empty string */
	str = git_oid_to_string(out, 1, &in);
	must_be_true(str && *str == '\0' && str == out);

	for (i = 1; i < GIT_OID_HEXSZ; i++) {
		out[i+1] = 'Z';
		str = git_oid_to_string(out, i+1, &in);
		/* returns out containing c-string */
		must_be_true(str && str == out);
		/* must be '\0' terminated */
		must_be_true(*(str+i) == '\0');
		/* must not touch bytes past end of string */
		must_be_true(*(str+(i+1)) == 'Z');
		/* i == n-1 charaters of string */
		must_pass(strncmp(exp, out, i));
	}

	/* returns out as hex formatted c-string */
	str = git_oid_to_string(out, sizeof(out), &in);
	must_be_true(str && str == out && *(str+GIT_OID_HEXSZ) == '\0');
	must_be_true(strcmp(exp, out) == 0);
END_TEST

BEGIN_TEST(oid15, "convert raw oid to string (big)")
	const char *exp = "16a0123456789abcdef4b775213c23a8bd74f5e0";
	git_oid in;
	char big[GIT_OID_HEXSZ + 1 + 3]; /* note + 4 => big buffer */
	char *str;

	must_pass(git_oid_fromstr(&in, exp));

	/* place some tail material */
	big[GIT_OID_HEXSZ+0] = 'W'; /* should be '\0' afterwards */
	big[GIT_OID_HEXSZ+1] = 'X'; /* should remain untouched   */
	big[GIT_OID_HEXSZ+2] = 'Y'; /* ditto */
	big[GIT_OID_HEXSZ+3] = 'Z'; /* ditto */

	/* returns big as hex formatted c-string */
	str = git_oid_to_string(big, sizeof(big), &in);
	must_be_true(str && str == big && *(str+GIT_OID_HEXSZ) == '\0');
	must_be_true(strcmp(exp, big) == 0);

	/* check tail material is untouched */
	must_be_true(str && str == big && *(str+GIT_OID_HEXSZ+1) == 'X');
	must_be_true(str && str == big && *(str+GIT_OID_HEXSZ+2) == 'Y');
	must_be_true(str && str == big && *(str+GIT_OID_HEXSZ+3) == 'Z');
END_TEST


BEGIN_TEST(oid16, "make sure the OID shortener doesn't choke on duplicate sha1s")

	git_oid_shorten *os;
	int min_len;

	os = git_oid_shorten_new(0);
	must_be_true(os != NULL);

	git_oid_shorten_add(os, "22596363b3de40b06f981fb85d82312e8c0ed511");
	git_oid_shorten_add(os, "ce08fe4884650f067bd5703b6a59a8b3b3c99a09");
	git_oid_shorten_add(os, "16a0123456789abcdef4b775213c23a8bd74f5e0");
	min_len = git_oid_shorten_add(os, "ce08fe4884650f067bd5703b6a59a8b3b3c99a09");

	must_be_true(min_len == GIT_OID_HEXSZ + 1);

	git_oid_shorten_free(os);
END_TEST

BEGIN_TEST(oid17, "stress test for the git_oid_shorten object")

#define MAX_OIDS 1000

	git_oid_shorten *os;
	char *oids[MAX_OIDS];
	char number_buffer[16];
	git_oid oid;
	size_t i, j;

	int min_len = 0, found_collision;

	os = git_oid_shorten_new(0);
	must_be_true(os != NULL);

	/*
	 * Insert in the shortener 1000 unique SHA1 ids
	 */
	for (i = 0; i < MAX_OIDS; ++i) {
		char *oid_text;

		sprintf(number_buffer, "%u", (unsigned int)i);
		git_hash_buf(&oid, number_buffer, strlen(number_buffer));

		oid_text = git__malloc(GIT_OID_HEXSZ + 1);
		git_oid_fmt(oid_text, &oid);
		oid_text[GIT_OID_HEXSZ] = 0;

		min_len = git_oid_shorten_add(os, oid_text);
		must_be_true(min_len >= 0);

		oids[i] = oid_text;
	}

	/*
	 * Compare the first `min_char - 1` characters of each
	 * SHA1 OID. If the minimizer worked, we should find at
	 * least one collision
	 */
	found_collision = 0;
	for (i = 0; i < MAX_OIDS; ++i) {
		for (j = 0; j < MAX_OIDS; ++j) {
			if (i != j && memcmp(oids[i], oids[j], min_len - 1) == 0)
				found_collision = 1;
		}
	}
	must_be_true(found_collision == 1);

	/*
	 * Compare the first `min_char` characters of each
	 * SHA1 OID. If the minimizer worked, every single preffix
	 * should be unique.
	 */
	found_collision = 0;
	for (i = 0; i < MAX_OIDS; ++i) {
		for (j = 0; j < MAX_OIDS; ++j) {
			if (i != j && memcmp(oids[i], oids[j], min_len) == 0)
				found_collision = 1;
		}
	}
	must_be_true(found_collision == 0);

	/* cleanup */
	for (i = 0; i < MAX_OIDS; ++i)
		git__free(oids[i]);

	git_oid_shorten_free(os);

#undef MAX_OIDS
END_TEST

static char *hello_id = "22596363b3de40b06f981fb85d82312e8c0ed511";
static char *hello_text = "hello world\n";

static char *bye_id = "ce08fe4884650f067bd5703b6a59a8b3b3c99a09";
static char *bye_text = "bye world\n";

BEGIN_TEST(hash0, "normal hash by blocks")
    git_hash_ctx *ctx;
    git_oid id1, id2;

    must_be_true((ctx = git_hash_new_ctx()) != NULL);

	/* should already be init'd */
    git_hash_update(ctx, hello_text, strlen(hello_text));
    git_hash_final(&id2, ctx);
    must_pass(git_oid_fromstr(&id1, hello_id));
    must_be_true(git_oid_cmp(&id1, &id2) == 0);

	/* reinit should permit reuse */
    git_hash_init(ctx);
    git_hash_update(ctx, bye_text, strlen(bye_text));
    git_hash_final(&id2, ctx);
    must_pass(git_oid_fromstr(&id1, bye_id));
    must_be_true(git_oid_cmp(&id1, &id2) == 0);

    git_hash_free_ctx(ctx);
END_TEST

BEGIN_TEST(hash1, "hash whole buffer in a single call")
    git_oid id1, id2;

    must_pass(git_oid_fromstr(&id1, hello_id));

    git_hash_buf(&id2, hello_text, strlen(hello_text));

    must_be_true(git_oid_cmp(&id1, &id2) == 0);
END_TEST

BEGIN_TEST(hash2, "hash a vector")
    git_oid id1, id2;
    git_buf_vec vec[2];

    must_pass(git_oid_fromstr(&id1, hello_id));

    vec[0].data = hello_text;
    vec[0].len  = 4;
    vec[1].data = hello_text+4;
    vec[1].len  = strlen(hello_text)-4;

    git_hash_vec(&id2, vec, 2);

    must_be_true(git_oid_cmp(&id1, &id2) == 0);
END_TEST

BEGIN_TEST(objtype0, "convert type to string")
	must_be_true(!strcmp(git_object_type2string(GIT_OBJ_BAD), ""));
	must_be_true(!strcmp(git_object_type2string(GIT_OBJ__EXT1), ""));
	must_be_true(!strcmp(git_object_type2string(GIT_OBJ_COMMIT), "commit"));
	must_be_true(!strcmp(git_object_type2string(GIT_OBJ_TREE), "tree"));
	must_be_true(!strcmp(git_object_type2string(GIT_OBJ_BLOB), "blob"));
	must_be_true(!strcmp(git_object_type2string(GIT_OBJ_TAG), "tag"));
	must_be_true(!strcmp(git_object_type2string(GIT_OBJ__EXT2), ""));
	must_be_true(!strcmp(git_object_type2string(GIT_OBJ_OFS_DELTA), "OFS_DELTA"));
	must_be_true(!strcmp(git_object_type2string(GIT_OBJ_REF_DELTA), "REF_DELTA"));

	must_be_true(!strcmp(git_object_type2string(-2), ""));
	must_be_true(!strcmp(git_object_type2string(8), ""));
	must_be_true(!strcmp(git_object_type2string(1234), ""));
END_TEST

BEGIN_TEST(objtype1, "convert string to type")
	must_be_true(git_object_string2type(NULL) == GIT_OBJ_BAD);
	must_be_true(git_object_string2type("") == GIT_OBJ_BAD);
	must_be_true(git_object_string2type("commit") == GIT_OBJ_COMMIT);
	must_be_true(git_object_string2type("tree") == GIT_OBJ_TREE);
	must_be_true(git_object_string2type("blob") == GIT_OBJ_BLOB);
	must_be_true(git_object_string2type("tag") == GIT_OBJ_TAG);
	must_be_true(git_object_string2type("OFS_DELTA") == GIT_OBJ_OFS_DELTA);
	must_be_true(git_object_string2type("REF_DELTA") == GIT_OBJ_REF_DELTA);

	must_be_true(git_object_string2type("CoMmIt") == GIT_OBJ_BAD);
	must_be_true(git_object_string2type("hohoho") == GIT_OBJ_BAD);
END_TEST

BEGIN_TEST(objtype2, "check if an object type is loose")
	must_be_true(git_object_typeisloose(GIT_OBJ_BAD) == 0);
	must_be_true(git_object_typeisloose(GIT_OBJ__EXT1) == 0);
	must_be_true(git_object_typeisloose(GIT_OBJ_COMMIT) == 1);
	must_be_true(git_object_typeisloose(GIT_OBJ_TREE) == 1);
	must_be_true(git_object_typeisloose(GIT_OBJ_BLOB) == 1);
	must_be_true(git_object_typeisloose(GIT_OBJ_TAG) == 1);
	must_be_true(git_object_typeisloose(GIT_OBJ__EXT2) == 0);
	must_be_true(git_object_typeisloose(GIT_OBJ_OFS_DELTA) == 0);
	must_be_true(git_object_typeisloose(GIT_OBJ_REF_DELTA) == 0);

	must_be_true(git_object_typeisloose(-2) == 0);
	must_be_true(git_object_typeisloose(8) == 0);
	must_be_true(git_object_typeisloose(1234) == 0);
END_TEST

BEGIN_TEST(objhash0, "hash junk data")
    git_oid id, id_zero;

    must_pass(git_oid_fromstr(&id_zero, zero_id));

    /* invalid types: */
    junk_obj.data = some_data;
    must_fail(hash_object(&id, &junk_obj));

    junk_obj.type = GIT_OBJ__EXT1;
    must_fail(hash_object(&id, &junk_obj));

    junk_obj.type = GIT_OBJ__EXT2;
    must_fail(hash_object(&id, &junk_obj));

    junk_obj.type = GIT_OBJ_OFS_DELTA;
    must_fail(hash_object(&id, &junk_obj));

    junk_obj.type = GIT_OBJ_REF_DELTA;
    must_fail(hash_object(&id, &junk_obj));

    /* data can be NULL only if len is zero: */
    junk_obj.type = GIT_OBJ_BLOB;
    junk_obj.data = NULL;
    must_pass(hash_object(&id, &junk_obj));
    must_be_true(git_oid_cmp(&id, &id_zero) == 0);

    junk_obj.len = 1;
    must_fail(hash_object(&id, &junk_obj));
END_TEST

BEGIN_TEST(objhash1, "hash a commit object")
    git_oid id1, id2;

    must_pass(git_oid_fromstr(&id1, commit_id));

    must_pass(hash_object(&id2, &commit_obj));

    must_be_true(git_oid_cmp(&id1, &id2) == 0);
END_TEST

BEGIN_TEST(objhash2, "hash a tree object")
    git_oid id1, id2;

    must_pass(git_oid_fromstr(&id1, tree_id));

    must_pass(hash_object(&id2, &tree_obj));

    must_be_true(git_oid_cmp(&id1, &id2) == 0);
END_TEST

BEGIN_TEST(objhash3, "hash a tag object")
    git_oid id1, id2;

    must_pass(git_oid_fromstr(&id1, tag_id));

    must_pass(hash_object(&id2, &tag_obj));

    must_be_true(git_oid_cmp(&id1, &id2) == 0);
END_TEST

BEGIN_TEST(objhash4, "hash a zero-length object")
    git_oid id1, id2;

    must_pass(git_oid_fromstr(&id1, zero_id));

    must_pass(hash_object(&id2, &zero_obj));

    must_be_true(git_oid_cmp(&id1, &id2) == 0);
END_TEST

BEGIN_TEST(objhash5, "hash an one-byte long object")
    git_oid id1, id2;

    must_pass(git_oid_fromstr(&id1, one_id));

    must_pass(hash_object(&id2, &one_obj));

    must_be_true(git_oid_cmp(&id1, &id2) == 0);
END_TEST

BEGIN_TEST(objhash6, "hash a two-byte long object")
    git_oid id1, id2;

    must_pass(git_oid_fromstr(&id1, two_id));

    must_pass(hash_object(&id2, &two_obj));

    must_be_true(git_oid_cmp(&id1, &id2) == 0);
END_TEST

BEGIN_TEST(objhash7, "hash an object several bytes long")
    git_oid id1, id2;

    must_pass(git_oid_fromstr(&id1, some_id));

    must_pass(hash_object(&id2, &some_obj));

    must_be_true(git_oid_cmp(&id1, &id2) == 0);
END_TEST

BEGIN_SUITE(rawobjects)
	ADD_TEST(oid0);
	ADD_TEST(oid1);
	ADD_TEST(oid2);
	ADD_TEST(oid3);
	ADD_TEST(oid4);
	ADD_TEST(oid5);
	ADD_TEST(oid6);
	ADD_TEST(oid7);
	ADD_TEST(oid8);
	ADD_TEST(oid9);
	ADD_TEST(oid10);
	ADD_TEST(oid11);
	ADD_TEST(oid12);
	ADD_TEST(oid13);
	ADD_TEST(oid14);
	ADD_TEST(oid15);
	ADD_TEST(oid16);
	ADD_TEST(oid17);

	ADD_TEST(hash0);
	ADD_TEST(hash1);
	ADD_TEST(hash2);

	ADD_TEST(objtype0);
	ADD_TEST(objtype1);
	ADD_TEST(objtype2);

	ADD_TEST(objhash0);
	ADD_TEST(objhash1);
	ADD_TEST(objhash2);
	ADD_TEST(objhash3);
	ADD_TEST(objhash4);
	ADD_TEST(objhash5);
	ADD_TEST(objhash6);
	ADD_TEST(objhash7);
END_SUITE

