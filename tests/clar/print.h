/* clap: clar protocol, the traditional clar output format */

static void clar_print_clap_init(int test_count, int suite_count, const char *suite_names)
{
	(void)test_count;
	printf("Loaded %d suites: %s\n", (int)suite_count, suite_names);
	printf("Started (test status codes: OK='.' FAILURE='F' SKIPPED='S')\n");
}

static void clar_print_clap_shutdown(int test_count, int suite_count, int error_count)
{
	(void)test_count;
	(void)suite_count;
	(void)error_count;

	printf("\n\n");
	clar_report_all();
}

static void clar_print_clap_error(int num, const struct clar_report *report, const struct clar_error *error)
{
	printf("  %d) Failure:\n", num);

	printf("%s::%s [%s:%"PRIuZ"]\n",
		report->suite,
		report->test,
		error->file,
		error->line_number);

	printf("  %s\n", error->error_msg);

	if (error->description != NULL)
		printf("  %s\n", error->description);

	printf("\n");
	fflush(stdout);
}

static void clar_print_clap_ontest(const char *test_name, int test_number, enum cl_test_status status)
{
	(void)test_name;
	(void)test_number;

	switch(status) {
	case CL_TEST_OK: printf("."); break;
	case CL_TEST_FAILURE: printf("F"); break;
	case CL_TEST_SKIP: printf("S"); break;
	case CL_TEST_NOTRUN: printf("N"); break;
	}

	fflush(stdout);
}

static void clar_print_clap_onsuite(const char *suite_name, int suite_index)
{
	if (_clar.report_suite_names)
		printf("\n%s", suite_name);

	(void)suite_index;
}

static void clar_print_clap_onabort(const char *fmt, va_list arg)
{
	vfprintf(stderr, fmt, arg);
}

/* indirection between protocol output selection */

#define PRINT(FN, args...) do { \
		switch (_clar.output_format) { \
			case CL_OUTPUT_CLAP: \
				clar_print_clap_##FN (args); \
				break; \
			default: \
				abort(); \
		} \
	} while (0)

static void clar_print_init(int test_count, int suite_count, const char *suite_names)
{
	PRINT(init, test_count, suite_count, suite_names);
}

static void clar_print_shutdown(int test_count, int suite_count, int error_count)
{
	PRINT(shutdown, test_count, suite_count, error_count);
}

static void clar_print_error(int num, const struct clar_report *report, const struct clar_error *error)
{
	PRINT(error, num, report, error);
}

static void clar_print_ontest(const char *test_name, int test_number, enum cl_test_status status)
{
	PRINT(ontest, test_name, test_number, status);
}

static void clar_print_onsuite(const char *suite_name, int suite_index)
{
	PRINT(onsuite, suite_name, suite_index);
}

static void clar_print_onabort(const char *msg, ...)
{
	va_list argp;
	va_start(argp, msg);
	PRINT(onabort, msg, argp);
	va_end(argp);
}
