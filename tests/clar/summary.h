
#include <fcntl.h>
#include <stdio.h>
#include <time.h>

int clar_summary_close_tag(
    struct clar_summary *summary, const char *tag, int indent)
{
	const char *indt;

	if (indent == 0) indt = "";
	else if (indent == 1) indt = "\t";
	else indt = "\t\t";

	return fprintf(summary->fp, "%s</%s>\n", indt, tag);
}

int clar_summary_testsuites(struct clar_summary *summary)
{
	if (summary->append)
		return 0;

	return fprintf(summary->fp, "<testsuites>\n");
}

int clar_summary_testsuite(struct clar_summary *summary,
    int idn, const char *name, const char *pkg, time_t timestamp,
    double elapsed, int test_count, int fail_count, int error_count)
{
	struct tm *tm = localtime(&timestamp);
	char iso_dt[20];

	if (strftime(iso_dt, sizeof(iso_dt), "%Y-%m-%dT%H:%M:%S", tm) == 0)
		return -1;

	return fprintf(summary->fp, "\t<testsuite "
		       " id=\"%d\""
		       " name=\"%s\""
		       " package=\"%s\""
		       " hostname=\"localhost\""
		       " timestamp=\"%s\""
		       " time=\"%.2f\""
		       " tests=\"%d\""
		       " failures=\"%d\""
		       " errors=\"%d\">\n",
		       idn, name, pkg, iso_dt, elapsed, test_count, fail_count, error_count);
}

int clar_summary_testcase(struct clar_summary *summary,
    const char *name, const char *classname, double elapsed)
{
	return fprintf(summary->fp,
	    "\t\t<testcase name=\"%s\" classname=\"%s\" time=\"%.2f\">\n",
		name, classname, elapsed);
}

int clar_summary_failure(struct clar_summary *summary,
    const char *type, const char *message, const char *desc)
{
	return fprintf(summary->fp,
	    "\t\t\t<failure type=\"%s\"><![CDATA[%s\n%s]]></failure>\n",
	    type, message, desc);
}

int clar_summary_init(
    struct clar_summary *summary,
    const char *filename,
	int append)
{
	int trunc = append ? 0 : O_TRUNC;
	const char *mode = append ? "w+" : "w";
	char closing[14];
	int fd;
	FILE *fp = NULL;

	memset(summary, 0, sizeof(struct clar_summary));

	/* open an fd so we can avoid truncation in the append case */
	if ((fd = open(filename, O_RDWR|O_CREAT|trunc, 0644)) < 0 ||
		(fp = fdopen(fd, mode)) == NULL) {
		summary->error_msg = "Failed to open summary file";
		goto on_error;
	}

	/*
	 * in append-mode, see if there's a closing root node
	 * ("</testsuites>\n") in the file already.  if so, remove it by
	 * positioning us to write there.
	 */
	if (append) {
		if (fseek(fp, 0, SEEK_END) < 0) {
			summary->error_msg = strerror(errno);
			goto on_error;
		}
	}

	if (append && ftell(fp) > 0) {
		if (fseek(fp, -14, SEEK_END) < 0) {
			summary->error_msg = "Failed to open summary file: file exists but is not JUnit XML";
			return -1;
		}

		if (fread(closing, 14, 1, fp) != 1) {
			summary->error_msg = strerror(errno);
			return -1;
		}

		if (memcmp(closing, "</testsuites>\n", 14) != 0) {
			summary->error_msg = "Failed to open summary file: file exists but is not JUnit XML";
			return -1;
		}

		if (fseek(fp, -14, SEEK_END) < 0) {
			summary->error_msg = "Failed to set position in summary file";
			return -1;
		}

		summary->append = 1;
	}

	summary->filename = filename;
	summary->fp = fp;
	return 0;

on_error:
	if (fp)
		fclose(fp);

	return -1;
}

int clar_summary_shutdown(struct clar_summary *summary)
{
	struct clar_report *report;
	const char *last_suite = NULL;

	if (clar_summary_testsuites(summary) < 0)
		goto on_error;

	report = _clar.reports;
	while (report != NULL) {
		struct clar_error *error = report->errors;

		if (last_suite == NULL || strcmp(last_suite, report->suite) != 0) {
			if (clar_summary_testsuite(summary, 0, report->suite, "",
			    time(NULL), 0, _clar.tests_ran, _clar.total_errors, 0) < 0)
				goto on_error;
		}

		last_suite = report->suite;

		clar_summary_testcase(summary, report->test, "what", 0);

		while (error != NULL) {
			if (clar_summary_failure(summary, "assert",
			    error->error_msg, error->description) < 0)
				goto on_error;

			error = error->next;
		}

		if (clar_summary_close_tag(summary, "testcase", 2) < 0)
			goto on_error;

		report = report->next;

		if (!report || strcmp(last_suite, report->suite) != 0) {
			if (clar_summary_close_tag(summary, "testsuite", 1) < 0)
				goto on_error;
		}
	}

	if (clar_summary_close_tag(summary, "testsuites", 0) < 0 ||
	    fclose(summary->fp) != 0)
		goto on_error;

	printf("written summary file to %s\n", summary->filename);
	return 0;

on_error:
	fclose(summary->fp);
	return -1;
}
