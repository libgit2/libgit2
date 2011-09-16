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
#define cl_must_pass(expr) cl_must_pass_((expr), NULL)
#define cl_must_fail(expr) cl_must_fail_((expr), NULL)
#define cl_assert(expr) cl_assert_((expr), NULL)

/**
 * Check macros with no error message
 */
#define cl_check_pass(expr) cl_check_pass_((expr), NULL)
#define cl_check_fail(expr) cl_check_fail_((expr), NULL)
#define cl_check(expr) cl_check_((expr), NULL)


/**
 * Forced failure/warning
 */
#define cl_fail(desc) clay__assert(0, __FILE__, __LINE__, "Test failed.", desc, 1)
#define cl_warning(desc) clay__assert(0, __FILE__, __LINE__, "Warning during test execution:", desc, 0)

#endif
