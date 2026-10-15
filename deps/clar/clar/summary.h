#include <stdio.h>
#include <time.h>

static int clar_summary_time_digits(double t)
{
	int digits = 3;

	if (t >= 100.0)
		return 1;
	else if (t >= 10.0)
		return 2;

	while (t > 0.0 && t < 1.0 && digits < 10) {
		t *= 10.0;
		digits++;
	}

	return digits;
}

static int clar_summary_junit_close_tag(
    struct clar_summary *summary, const char *tag, int indent)
{
	const char *indt;

	if (indent == 0) indt = "";
	else if (indent == 1) indt = "\t";
	else indt = "\t\t";

	return fprintf(summary->fp, "%s</%s>\n", indt, tag);
}

static int clar_summary_junit_testsuites(struct clar_summary *summary)
{
	return fprintf(summary->fp, "<testsuites>\n");
}

static int clar_summary_junit_testsuite(struct clar_summary *summary,
    int idn, const char *name, time_t timestamp,
    int test_count, int fail_count, int error_count)
{
	struct tm tm;
	char iso_dt[20];

	localtime_r(&timestamp, &tm);
	if (strftime(iso_dt, sizeof(iso_dt), "%Y-%m-%dT%H:%M:%S", &tm) == 0)
		return -1;

	return fprintf(summary->fp, "\t<testsuite"
		       " id=\"%d\""
		       " name=\"%s\""
		       " hostname=\"localhost\""
		       " timestamp=\"%s\""
		       " tests=\"%d\""
		       " failures=\"%d\""
		       " errors=\"%d\">\n",
		       idn, name, iso_dt, test_count, fail_count, error_count);
}

static int clar_summary_junit_testcase(struct clar_summary *summary,
    const char *name, const char *classname, double elapsed)
{
	return fprintf(summary->fp,
	    "\t\t<testcase name=\"%s\" classname=\"%s\" time=\"%.*f\">\n",
		name, classname, clar_summary_time_digits(elapsed), elapsed);
}

static int clar_summary_junit_failure(struct clar_summary *summary,
    const char *type, const char *message, const char *desc)
{
	return fprintf(summary->fp,
	    "\t\t\t<failure type=\"%s\"><![CDATA[%s\n%s]]></failure>\n",
	    type, message, desc);
}

static int clar_summary_junit_skipped(struct clar_summary *summary)
{
	return fprintf(summary->fp, "\t\t\t<skipped />\n");
}

static struct clar_summary *clar_summary_junit_init(const char *filename)
{
	struct clar_summary *summary;
	FILE *fp;

	if ((fp = fopen(filename, "w")) == NULL) {
		perror("fopen");
		return NULL;
	}

	if ((summary = malloc(sizeof(struct clar_summary))) == NULL) {
		perror("malloc");
		fclose(fp);
		return NULL;
	}

	summary->filename = filename;
	summary->fp = fp;

	return summary;
}

static int clar_summary_junit_shutdown(struct clar_summary *summary)
{
	struct clar_report *report;
	const char *last_suite = NULL;

	if (clar_summary_junit_testsuites(summary) < 0)
		goto on_error;

	report = _clar.reports;
	while (report != NULL) {
		struct clar_error *error = report->errors;

		if (last_suite == NULL || strcmp(last_suite, report->suite) != 0) {
			if (clar_summary_junit_testsuite(summary, 0, report->suite,
			    report->start, _clar.tests_ran, _clar.total_errors, 0) < 0)
				goto on_error;
		}

		last_suite = report->suite;

		clar_summary_junit_testcase(summary, report->test, report->suite, report->time_total);

		while (error != NULL) {
			if (clar_summary_junit_failure(summary, "assert",
			    error->message, error->description) < 0)
				goto on_error;

			error = error->next;
		}

		if (report->status == CL_TEST_SKIP)
			clar_summary_junit_skipped(summary);

		if (clar_summary_junit_close_tag(summary, "testcase", 2) < 0)
			goto on_error;

		report = report->next;

		if (!report || strcmp(last_suite, report->suite) != 0) {
			if (clar_summary_junit_close_tag(summary, "testsuite", 1) < 0)
				goto on_error;
		}
	}

	if (clar_summary_junit_close_tag(summary, "testsuites", 0) < 0 ||
	    fclose(summary->fp) != 0)
		goto on_error;

	printf("written summary file to %s\n", summary->filename);

	free(summary);
	return 0;

on_error:
	fclose(summary->fp);
	free(summary);
	return -1;
}

static struct clar_summary *clar_summary_json_init(const char *filename)
{
	struct clar_summary *summary;
	FILE *fp;

	if ((fp = fopen(filename, "w")) == NULL) {
		perror("fopen");
		return NULL;
	}

	if ((summary = malloc(sizeof(struct clar_summary))) == NULL) {
		perror("malloc");
		fclose(fp);
		return NULL;
	}

	summary->filename = filename;
	summary->fp = fp;

	return summary;
}

static int clar_summary_json_shutdown(struct clar_summary *summary)
{
	struct clar_report *report;
	int i;

	fprintf(summary->fp, "{\n");
	fprintf(summary->fp, "  \"tests\": [\n");

	report = _clar.reports;
	while (report != NULL) {
		struct clar_error *error = report->errors;

		if (report != _clar.reports)
			fprintf(summary->fp, ",\n");

		fprintf(summary->fp, "    {\n");
		fprintf(summary->fp, "      \"name\": \"%s::%s\",\n", report->suite, report->test);

		if (report->description)
			fprintf(summary->fp, "      \"description\": \"%s\",\n", report->description);

		fprintf(summary->fp, "      \"results\": {\n");

		fprintf(summary->fp, "        \"status\": ");
		if (report->status == CL_TEST_OK)
			fprintf(summary->fp, "\"ok\",\n");
		else if (report->status == CL_TEST_FAILURE)
			fprintf(summary->fp, "\"failed\",\n");
		else if (report->status == CL_TEST_SKIP)
			fprintf(summary->fp, "\"skipped\"\n");
		else
			clar_abort("unknown test status %d", report->status);

		if (report->status == CL_TEST_OK) {
			fprintf(summary->fp, "        \"mean\": %.*f,\n",
				clar_summary_time_digits(report->time_mean), report->time_mean);
			fprintf(summary->fp, "        \"stddev\": %.*f,\n",
				clar_summary_time_digits(report->time_stddev), report->time_stddev);
			fprintf(summary->fp, "        \"min\": %.*f,\n",
				clar_summary_time_digits(report->time_min), report->time_min);
			fprintf(summary->fp, "        \"max\": %.*f,\n",
				clar_summary_time_digits(report->time_max), report->time_max);
			fprintf(summary->fp, "        \"times\": [\n");

			for (i = 0; i < report->runs; i++) {
				if (i > 0)
					fprintf(summary->fp, ",\n");

				fprintf(summary->fp, "          %.*f",
					clar_summary_time_digits(report->times[i]), report->times[i]);
			}

			fprintf(summary->fp, "\n        ]\n");
		}

		if (report->status == CL_TEST_FAILURE) {
			fprintf(summary->fp, "        \"errors\": [\n");

			while (error != NULL) {
				if (error != report->errors)
					fprintf(summary->fp, ",\n");

				fprintf(summary->fp, "          {\n");
				fprintf(summary->fp, "            \"message\": \"%s\",\n", error->message);

				if (error->description)
					fprintf(summary->fp, "            \"description\": \"%s\",\n", error->description);

				fprintf(summary->fp, "            \"function\": \"%s\",\n", error->function);
				fprintf(summary->fp, "            \"file\": \"%s\",\n", error->file);
				fprintf(summary->fp, "            \"line\": %" PRIuMAX "\n", error->line_number);
				fprintf(summary->fp, "          }");

				error = error->next;
			}

			fprintf(summary->fp, "\n");
			fprintf(summary->fp, "        ]\n");
		}

		fprintf(summary->fp, "      }\n");
		fprintf(summary->fp, "    }");

		report = report->next;
	}

	fprintf(summary->fp, "\n");
	fprintf(summary->fp, "  ]\n");
	fprintf(summary->fp, "}\n");

	if (fclose(summary->fp) != 0)
		goto on_error;

	printf("written summary file to %s\n", summary->filename);

	free(summary);
	return 0;

on_error:
	fclose(summary->fp);
	free(summary);
	return -1;
}

/* indirection between protocol output selection */

#define SUMMARY(FN, ...) do { \
		switch (_clar.summary_format) { \
			case CL_SUMMARY_JUNIT: \
				return clar_summary_junit_##FN (__VA_ARGS__); \
				break; \
			case CL_SUMMARY_JSON: \
				return clar_summary_json_##FN (__VA_ARGS__); \
				break; \
			default: \
				abort(); \
		} \
	} while(0)

struct clar_summary *clar_summary_init(const char *filename)
{
	SUMMARY(init, filename);
}

int clar_summary_shutdown(struct clar_summary *summary)
{
	SUMMARY(shutdown, summary);
}
