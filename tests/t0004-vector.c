#include "test_lib.h"
#include "common.h"
#include "vector.h"

/* Initial size of 1 will cause writing past array bounds prior to fix */
BEGIN_TEST(initial_size_one)
  git_vector x;
  int i;
  git_vector_init(&x, 1, NULL, NULL);
  for (i = 0; i < 10; ++i) {
    git_vector_insert(&x, (void*) 0xabc);
  }
  git_vector_free(&x);
END_TEST

/* vector used to read past array bounds on remove() */
BEGIN_TEST(remove)
  git_vector x;
  // make initial capacity exact for our insertions.
  git_vector_init(&x, 3, NULL, NULL);
  git_vector_insert(&x, (void*) 0xabc);
  git_vector_insert(&x, (void*) 0xdef);
  git_vector_insert(&x, (void*) 0x123);

  git_vector_remove(&x, 0);  // used to read past array bounds.
  git_vector_free(&x);
END_TEST
