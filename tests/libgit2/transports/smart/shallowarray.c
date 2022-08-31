#include "clar_libgit2.h"

#include "git2/oid.h"
#include "git2/transport.h"

#include "common.h"
#include "transports/smart.h"
#include "oid.h"

#include <assert.h>

#define oid_0 "c070ad8c08840c8116da865b2d65593a6bb9cd2a"
#define oid_1 "0966a434eb1a025db6b71485ab63a3bfbea520b6"
#define oid_2 "83834a7afdaa1a1260568567f6ad90020389f664"

void test_transports_smart_shallowarray__add_and_remove_oid_from_shallowarray(void)
{
    git_oid oid_0_obj, oid_1_obj, oid_2_obj;
    git_shallowarray *shallow_roots = git__malloc(sizeof(git_shallowarray));
    git_array_init(shallow_roots->array);

    git_oid__fromstr(&oid_0_obj, oid_0, GIT_OID_SHA1);
    git_oid__fromstr(&oid_1_obj, oid_1, GIT_OID_SHA1);
    git_oid__fromstr(&oid_2_obj, oid_2, GIT_OID_SHA1);

    git_shallowarray_add(shallow_roots, &oid_0_obj);
    git_shallowarray_add(shallow_roots, &oid_1_obj);
    git_shallowarray_add(shallow_roots, &oid_2_obj);

    cl_assert_equal_i(3, shallow_roots->array.size);
	cl_assert_equal_s("c070ad8c08840c8116da865b2d65593a6bb9cd2a", git_oid_tostr_s(&shallow_roots->array.ptr[0]));
	cl_assert_equal_s("0966a434eb1a025db6b71485ab63a3bfbea520b6", git_oid_tostr_s(&shallow_roots->array.ptr[1]));
	cl_assert_equal_s("83834a7afdaa1a1260568567f6ad90020389f664", git_oid_tostr_s(&shallow_roots->array.ptr[2]));

    git_shallowarray_remove(shallow_roots, &oid_2_obj);

    cl_assert_equal_i(2, shallow_roots->array.size);
	cl_assert_equal_s("c070ad8c08840c8116da865b2d65593a6bb9cd2a", git_oid_tostr_s(&shallow_roots->array.ptr[0]));
	cl_assert_equal_s("0966a434eb1a025db6b71485ab63a3bfbea520b6", git_oid_tostr_s(&shallow_roots->array.ptr[1]));

    git_shallowarray_remove(shallow_roots, &oid_1_obj);

    cl_assert_equal_i(1, shallow_roots->array.size);
	cl_assert_equal_s("c070ad8c08840c8116da865b2d65593a6bb9cd2a", git_oid_tostr_s(&shallow_roots->array.ptr[0]));

    git_shallowarray_remove(shallow_roots, &oid_0_obj);

    cl_assert_equal_i(0, shallow_roots->array.size);

	git_array_clear(shallow_roots->array);
	git__free(shallow_roots);
}
