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

void clay_set_cleanup(void (*cleanup)(void *), void *opaque);

#define clay_must_pass(expr, desc) clay__assert((expr) >= 0, __FILE__, __LINE__, "Function call failed: " #expr, desc, 1)
#define clay_must_fail(expr, desc) clay__assert((expr) < 0, __FILE__, __LINE__, "Expected function call to fail: " #expr, desc, 1)
#define clay_assert(expr, desc) clay__assert((expr) != 0, __FILE__, __LINE__, "Expression is not true: " #expr, desc, 1)

#define clay_check_pass(expr, desc) clay__assert((expr) >= 0, __FILE__, __LINE__, "Function call failed: " #expr, desc, 0)
#define clay_check_fail(expr, desc) clay__assert((expr) < 0, __FILE__, __LINE__, "Expected function call to fail: " #expr, desc, 0)
#define clay_check(expr, desc) clay__assert((expr) != 0, __FILE__, __LINE__, "Expression is not true: " #expr, desc, 0)

#define clay_fail(desc) clay__assert(0, __FILE__, __LINE__, "Test failed.", desc, 1)
#define clay_warning(desc) clay__assert(0, __FILE__, __LINE__, "Warning during test execution:", desc, 0)

#endif
