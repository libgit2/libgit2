
static void clar_print_init(int test_count, int suite_count, const char *suite_names)
{
	(void)suite_names;
	printf("Loaded %d suites, with %d tests, starting\n", suite_count, test_count);
	printf("test status codes: OK='.' FAILURE='F' SKIPPED='S'\n");
}

static void clar_print_shutdown(const double evaluation_duration,
    int test_count, int suite_count, int error_count, int skip_count)
{
	int success_count;
	printf("\n\n");
	clar_report_errors();

	if (_clar.verbosity > 1) {
		if (test_count == 0)
			printf("no tests found");
		else {
			printf("\nran %d [", test_count);
			success_count = test_count - skip_count - error_count;
			if (success_count)
				printf("%d passed", success_count);
			if (skip_count)
				printf(" %d skipped", skip_count);
			if (error_count)
				printf(" %d failed", error_count);
			printf("] in %d suits", suite_count);
			printf(" in %.4f seconds", evaluation_duration);
		}
		printf("\n");
	}
}

static void clar_print_error(int num, const struct clar_error *error)
{
	printf("  %d) Failure:\n", num);

	printf("%s::%s [%s:%d]\n",
		error->suite,
		error->test,
		error->file,
		error->line_number);

	printf("  %s\n", error->error_msg);

	if (error->description != NULL)
		printf("  %s\n", error->description);

	printf("\n");
	fflush(stdout);
}

static void clar_print_ontest(const char *test_name, int test_number, enum cl_test_status status)
{
	if (_clar.verbosity > 1)
		printf("\n  %s [#%d] ", test_name, test_number);

	switch(status) {
	case CL_TEST_OK: printf("."); break;
	case CL_TEST_FAILURE: printf("F"); break;
	case CL_TEST_SKIP: printf("S"); break;
	}

	fflush(stdout);
}

static void clar_print_onsuite(const char *suite_name, int suite_index, size_t tests_in_suite)
{
	if (_clar.verbosity > 0 || _clar.report_suite_names)
		printf("\n%s", suite_name);

	if (_clar.verbosity > 1)
		printf(" [#%d with %zu tests]", suite_index, tests_in_suite);
}

static void clar_print_onabort(const char *msg, ...)
{
	va_list argp;
	va_start(argp, msg);
	vfprintf(stderr, msg, argp);
	va_end(argp);
}
