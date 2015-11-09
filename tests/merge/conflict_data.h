#define AUTOMERGEABLE_MERGED_FILE \
	"this file is changed in master\n" \
	"this file is automergeable\n" \
	"this file is automergeable\n" \
	"this file is automergeable\n" \
	"this file is automergeable\n" \
	"this file is automergeable\n" \
	"this file is automergeable\n" \
	"this file is automergeable\n" \
	"this file is changed in branch\n"

#define AUTOMERGEABLE_MERGED_FILE_CRLF \
	"this file is changed in master\r\n" \
	"this file is automergeable\r\n" \
	"this file is automergeable\r\n" \
	"this file is automergeable\r\n" \
	"this file is automergeable\r\n" \
	"this file is automergeable\r\n" \
	"this file is automergeable\r\n" \
	"this file is automergeable\r\n" \
	"this file is changed in branch\r\n"

#define CONFLICTING_MERGE_FILE \
	"<<<<<<< HEAD\n" \
	"this file is changed in master and branch\n" \
	"=======\n" \
	"this file is changed in branch and master\n" \
	">>>>>>> 7cb63eed597130ba4abb87b3e544b85021905520\n"

#define CONFLICTING_DIFF3_FILE \
	"<<<<<<< HEAD\n" \
	"this file is changed in master and branch\n" \
	"||||||| initial\n" \
	"this file is a conflict\n" \
	"=======\n" \
	"this file is changed in branch and master\n" \
	">>>>>>> 7cb63eed597130ba4abb87b3e544b85021905520\n"

#define CONFLICTING_UNION_FILE \
	"this file is changed in master and branch\n" \
	"this file is changed in branch and master\n"

