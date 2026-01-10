/* clap: clar protocol, the traditional clar output format */

static void clar_print_clap_init(int test_count, int suite_count)
{
	(void)test_count;

	if (_clar.verbosity < 0)
		return;

	printf("Loaded %d suites:\n", (int)suite_count);
	printf("Started (test status codes: OK='.' FAILURE='F' SKIPPED='S')\n");
}

static void clar_print_clap_shutdown(int test_count, int suite_count, int error_count)
{
	(void)test_count;
	(void)suite_count;
	(void)error_count;

	if (_clar.verbosity >= 0)
		printf("\n\n");
	clar_report_all();
}


static void clar_print_indented(const char *str, int indent)
{
	const char *bol, *eol;

	for (bol = str; *bol; bol = eol) {
		eol = strchr(bol, '\n');
		if (eol)
			eol++;
		else
			eol = bol + strlen(bol);
		printf("%*s%.*s", indent, "", (int)(eol - bol), bol);
	}
	putc('\n', stdout);
}

static void clar_print_clap_error(int num, const struct clar_report *report, const struct clar_error *error)
{
	printf("  %d) Failure:\n", num);

	printf("%s::%s [%s:%"PRIuMAX"]\n",
		report->suite,
		report->test,
		error->file,
		error->line_number);

	clar_print_indented(error->message, 2);

	if (error->description != NULL)
		clar_print_indented(error->description, 2);

	printf("\n");
	fflush(stdout);
}

static void clar_print_clap_test_start(const char *suite_name, const char *test_name, int test_number)
{
	(void)test_number;

	if (_clar.verbosity < 0)
		return;

	if (_clar.verbosity > 1) {
		printf("%s::%s: ", suite_name, test_name);
		fflush(stdout);
	}
}

static void clar_print_clap_test_finish(const char *suite_name, const char *test_name, int test_number, const struct clar_report *report)
{
	(void)suite_name;
	(void)test_name;
	(void)test_number;

	if (_clar.verbosity == 0) {
		switch (report->status) {
		case CL_TEST_OK: printf("."); break;
		case CL_TEST_FAILURE: printf("F"); break;
		case CL_TEST_SKIP: printf("S"); break;
		case CL_TEST_NOTRUN: printf("N"); break;
		}

		fflush(stdout);
	} else if (_clar.verbosity > 1) {
		switch (report->status) {
		case CL_TEST_OK: printf("ok\n"); break;
		case CL_TEST_FAILURE: printf("fail\n"); break;
		case CL_TEST_SKIP: printf("skipped\n"); break;
		case CL_TEST_NOTRUN: printf("notrun\n"); break;
		}
	}
}

static void clar_print_clap_suite_start(const char *suite_name, int suite_index)
{
	if (_clar.verbosity < 0)
		return;
	if (_clar.verbosity == 1)
		printf("\n%s", suite_name);

	(void)suite_index;
}

static void clar_print_clap_onabort(const char *fmt, va_list arg)
{
	vfprintf(stderr, fmt, arg);
}

/* tap: test anywhere protocol format */

static void clar_print_tap_init(int test_count, int suite_count)
{
	(void)test_count;
	(void)suite_count;
	printf("TAP version 13\n");
}

static void clar_print_tap_shutdown(int test_count, int suite_count, int error_count)
{
	(void)suite_count;
	(void)error_count;

	printf("1..%d\n", test_count);
}

static void clar_print_tap_error(int num, const struct clar_report *report, const struct clar_error *error)
{
	(void)num;
	(void)report;
	(void)error;
}

static void print_escaped(const char *str)
{
	char *c;

	while ((c = strchr(str, '\'')) != NULL) {
		printf("%.*s", (int)(c - str), str);
		printf("''");
		str = c + 1;
	}

	printf("%s", str);
}

static void clar_print_tap_test_start(const char *suite_name, const char *test_name, int test_number)
{
	(void)suite_name;
	(void)test_name;
	(void)test_number;
}

static void clar_print_tap_test_finish(const char *suite_name, const char *test_name, int test_number, const struct clar_report *report)
{
	const struct clar_error *error = _clar.last_report->errors;

	(void)test_name;
	(void)test_number;

	switch(report->status) {
	case CL_TEST_OK:
		printf("ok %d - %s::%s\n", test_number, suite_name, test_name);
		break;
	case CL_TEST_FAILURE:
		printf("not ok %d - %s::%s\n", test_number, suite_name, test_name);

		if (_clar.verbosity >= 0) {
			printf("    ---\n");
			printf("    reason: |\n");
			clar_print_indented(error->message, 6);

			if (error->description)
				clar_print_indented(error->description, 6);

			printf("    at:\n");
			printf("      file: '"); print_escaped(error->file); printf("'\n");
			printf("      line: %" PRIuMAX "\n", error->line_number);
			printf("      function: '%s'\n", error->function);
			printf("    ...\n");
		}

		break;
	case CL_TEST_SKIP:
	case CL_TEST_NOTRUN:
		printf("ok %d - # SKIP %s::%s\n", test_number, suite_name, test_name);
		break;
	}

	fflush(stdout);
}

static void clar_print_tap_suite_start(const char *suite_name, int suite_index)
{
	if (_clar.verbosity < 0)
		return;
	printf("# start of suite %d: %s\n", suite_index, suite_name);
}

static void clar_print_tap_onabort(const char *fmt, va_list arg)
{
	printf("Bail out! ");
	vprintf(fmt, arg);
	fflush(stdout);
}

/* timings format: useful for benchmarks */

static void clar_print_timing_init(int test_count, int suite_count)
{
	(void)test_count;
	(void)suite_count;

	printf("Started benchmarks (mean time ± stddev / min time … max time):\n\n");
}

static void clar_print_timing_shutdown(int test_count, int suite_count, int error_count)
{
	(void)test_count;
	(void)suite_count;
	(void)error_count;
}

static void clar_print_timing_error(int num, const struct clar_report *report, const struct clar_error *error)
{
	(void)num;
	(void)report;
	(void)error;
}

static void clar_print_timing_test_start(const char *suite_name, const char *test_name, int test_number)
{
	(void)test_number;

	printf("%s::%s:  ", suite_name, test_name);
	fflush(stdout);
}

static void clar_print_timing_time(double t)
{
	static const char *units[] = { "sec", "ms", "μs", "ns" };
	static const int units_len = sizeof(units) / sizeof(units[0]);
	int unit = 0, exponent = 0, digits;

	while (t < 1.0 && unit < units_len - 1) {
		t *= 1000.0;
		unit++;
	}

	while (t > 0.0 && t < 1.0 && exponent < 10) {
		t *= 10.0;
		exponent++;
	}

	digits = (t < 10.0) ? 3 : ((t < 100.0) ? 2 : 1);

	printf("%.*f", digits, t);

	if (exponent > 0)
		printf("e-%d", exponent);

	printf(" %s", units[unit]);
}

static void clar_print_timing_test_finish(const char *suite_name, const char *test_name, int test_number, const struct clar_report *report)
{
	const struct clar_error *error = _clar.last_report->errors;

	(void)suite_name;
	(void)test_name;
	(void)test_number;

	switch(report->status) {
	case CL_TEST_OK:
		clar_print_timing_time(report->time_mean);

		if (report->runs > 1) {
			printf(" ± ");
			clar_print_timing_time(report->time_stddev);

			printf(" / range: ");
			clar_print_timing_time(report->time_min);
			printf(" … ");
			clar_print_timing_time(report->time_max);
			printf("  (%d runs)", report->runs);
		}

		printf("\n");
		break;
	case CL_TEST_FAILURE:
		printf("failed: %s\n", error->message);
		break;
	case CL_TEST_SKIP:
	case CL_TEST_NOTRUN:
		printf("skipped\n");
		break;
	}

	fflush(stdout);
}

static void clar_print_timing_suite_start(const char *suite_name, int suite_index)
{
	if (_clar.verbosity == 1)
		printf("\n%s", suite_name);

	(void)suite_index;
}

static void clar_print_timing_onabort(const char *fmt, va_list arg)
{
	vfprintf(stderr, fmt, arg);
}

/* indirection between protocol output selection */

#define PRINT(FN, ...) do { \
		switch (_clar.output_format) { \
			case CL_OUTPUT_CLAP: \
				clar_print_clap_##FN (__VA_ARGS__); \
				break; \
			case CL_OUTPUT_TAP: \
				clar_print_tap_##FN (__VA_ARGS__); \
				break; \
			case CL_OUTPUT_TIMING: \
				clar_print_timing_##FN (__VA_ARGS__); \
				break; \
			default: \
				abort(); \
		} \
	} while (0)

static void clar_print_init(int test_count, int suite_count)
{
	PRINT(init, test_count, suite_count);
}

static void clar_print_shutdown(int test_count, int suite_count, int error_count)
{
	PRINT(shutdown, test_count, suite_count, error_count);
}

static void clar_print_error(int num, const struct clar_report *report, const struct clar_error *error)
{
	PRINT(error, num, report, error);
}

static void clar_print_test_start(const char *suite_name, const char *test_name, int test_number)
{
	PRINT(test_start, suite_name, test_name, test_number);
}

static void clar_print_test_finish(const char *suite_name, const char *test_name, int test_number, const struct clar_report *report)
{
	PRINT(test_finish, suite_name, test_name, test_number, report);
}

static void clar_print_suite_start(const char *suite_name, int suite_index)
{
	PRINT(suite_start, suite_name, suite_index);
}

static void clar_print_onabortv(const char *msg, va_list argp)
{
	PRINT(onabort, msg, argp);
}

static void clar_print_onabort(const char *msg, ...)
{
	va_list argp;
	va_start(argp, msg);
	clar_print_onabortv(msg, argp);
	va_end(argp);
}
