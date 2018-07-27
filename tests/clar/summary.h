
#include <stdio.h>
#include <time.h>

static FILE *summary;

int clar_summary_close_tag(const char *tag, int indent)
{
	const char *indt;

	if (indent == 0) indt = "";
	else if (indent == 1) indt = "\t";
	else indt = "\t\t";

	return fprintf(summary, "%s</%s>\n", indt, tag);
}

int clar_summary_testsuites(void)
{
	return fprintf(summary, "<testsuites>\n");
}

int clar_summary_testsuite(int idn, const char *name, const char *pkg, time_t timestamp, double time, int test_count, int fail_count, int error_count)
{
	struct tm *tm = localtime(&timestamp);
	char iso_dt[20];

	if (strftime(iso_dt, sizeof(iso_dt), "%FT%T", tm) == 0)
		return -1;

	return fprintf(summary, "\t<testsuite "
		       " id=\"%d\""
		       " name=\"%s\""
		       " package=\"%s\""
		       " hostname=\"localhost\""
		       " timestamp=\"%s\""
		       " time=\"%.2f\""
		       " tests=\"%d\""
		       " failures=\"%d\""
		       " errors=\"%d\">\n",
		       idn, name, pkg, iso_dt, time, test_count, fail_count, error_count);
}

int clar_summary_testcase(const char *name, const char *classname, double time)
{
	return fprintf(summary, "\t\t<testcase name=\"%s\" classname=\"%s\" time=\"%.2f\">\n", name, classname, time);
}

int clar_summary_failure(const char *type, const char *message, const char *desc)
{
	return fprintf(summary, "\t\t\t<failure type=\"%s\"><![CDATA[%s\n%s]]></failure>\n", type, message, desc);
}

void clar_summary_write(void)
{
	struct clar_report *report;
	const char *last_suite = NULL;
	char wd[1024];

	summary = fopen("summary.xml", "w");
	if (!summary) {
		printf("failed to open summary.xml for writing\n");
		return;
	}

	clar_summary_testsuites();

	report = _clar.reports;
	while (report != NULL) {
		struct clar_error *error = report->errors;

		if (last_suite == NULL || strcmp(last_suite, report->suite) != 0) {
			clar_summary_testsuite(0, report->suite, "", time(NULL), 0, _clar.tests_ran, _clar.total_errors, 0);
		}

		last_suite = report->suite;

		clar_summary_testcase(report->test, "what", 0);

		while (error != NULL) {
			clar_summary_failure("assert", error->error_msg, error->description);
			error = error->next;
		}

		clar_summary_close_tag("testcase", 2);

		report = report->next;

		if (!report || strcmp(last_suite, report->suite) != 0)
			clar_summary_close_tag("testsuite", 1);
	}

	clar_summary_close_tag("testsuites", 0);

	fclose(summary);

	printf("written summary file to %s\n", getcwd((char *)&wd, sizeof(wd)));
}
