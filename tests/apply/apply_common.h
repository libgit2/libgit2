/* The original file contents */

#define FILE_ORIGINAL \
	"hey!\n" \
	"this is some context!\n" \
	"around some lines\n" \
	"that will change\n" \
	"yes it is!\n" \
	"(this line is changed)\n" \
	"and this\n" \
	"is additional context\n" \
	"below it!\n"

/* A change in the middle of the file (and the resultant patch) */

#define FILE_CHANGE_MIDDLE \
	"hey!\n" \
	"this is some context!\n" \
	"around some lines\n" \
	"that will change\n" \
	"yes it is!\n" \
	"(THIS line is changed!)\n" \
	"and this\n" \
	"is additional context\n" \
	"below it!\n"

#define PATCH_ORIGINAL_TO_CHANGE_MIDDLE \
	"diff --git a/file.txt b/file.txt\n" \
	"index 9432026..cd8fd12 100644\n" \
	"--- a/file.txt\n" \
	"+++ b/file.txt\n" \
	"@@ -3,7 +3,7 @@ this is some context!\n" \
	" around some lines\n" \
	" that will change\n" \
	" yes it is!\n" \
	"-(this line is changed)\n" \
	"+(THIS line is changed!)\n" \
	" and this\n" \
	" is additional context\n" \
	" below it!\n"

#define PATCH_ORIGINAL_TO_CHANGE_MIDDLE_NOCONTEXT \
	"diff --git a/file.txt b/file.txt\n" \
	"index 9432026..cd8fd12 100644\n" \
	"--- a/file.txt\n" \
	"+++ b/file.txt\n" \
	"@@ -6 +6 @@ yes it is!\n" \
	"-(this line is changed)\n" \
	"+(THIS line is changed!)\n"

/* A change of the first line (and the resultant patch) */

#define FILE_CHANGE_FIRSTLINE \
	"hey, change in head!\n" \
	"this is some context!\n" \
	"around some lines\n" \
	"that will change\n" \
	"yes it is!\n" \
	"(this line is changed)\n" \
	"and this\n" \
	"is additional context\n" \
	"below it!\n"

#define PATCH_ORIGINAL_TO_CHANGE_FIRSTLINE \
	"diff --git a/file.txt b/file.txt\n" \
	"index 9432026..c81df1d 100644\n" \
	"--- a/file.txt\n" \
	"+++ b/file.txt\n" \
	"@@ -1,4 +1,4 @@\n" \
	"-hey!\n" \
	"+hey, change in head!\n" \
	" this is some context!\n" \
	" around some lines\n" \
	" that will change\n"

/* A change of the last line (and the resultant patch) */

#define FILE_CHANGE_LASTLINE \
	"hey!\n" \
	"this is some context!\n" \
	"around some lines\n" \
	"that will change\n" \
	"yes it is!\n" \
	"(this line is changed)\n" \
	"and this\n" \
	"is additional context\n" \
	"change to the last line.\n"

#define PATCH_ORIGINAL_TO_CHANGE_LASTLINE \
	"diff --git a/file.txt b/file.txt\n" \
	"index 9432026..f70db1c 100644\n" \
	"--- a/file.txt\n" \
	"+++ b/file.txt\n" \
	"@@ -6,4 +6,4 @@ yes it is!\n" \
	" (this line is changed)\n" \
	" and this\n" \
	" is additional context\n" \
	"-below it!\n" \
	"+change to the last line.\n"

/* An insertion at the beginning of the file (and the resultant patch) */

#define FILE_PREPEND \
	"insert at front\n" \
	"hey!\n" \
	"this is some context!\n" \
	"around some lines\n" \
	"that will change\n" \
	"yes it is!\n" \
	"(this line is changed)\n" \
	"and this\n" \
	"is additional context\n" \
	"below it!\n"

#define PATCH_ORIGINAL_TO_PREPEND \
	"diff --git a/file.txt b/file.txt\n" \
	"index 9432026..0f39b9a 100644\n" \
	"--- a/file.txt\n" \
	"+++ b/file.txt\n" \
	"@@ -1,3 +1,4 @@\n" \
	"+insert at front\n" \
	" hey!\n" \
	" this is some context!\n" \
	" around some lines\n"

#define PATCH_ORIGINAL_TO_PREPEND_NOCONTEXT \
	"diff --git a/file.txt b/file.txt\n" \
	"index 9432026..0f39b9a 100644\n" \
	"--- a/file.txt\n" \
	"+++ b/file.txt\n" \
	"@@ -0,0 +1 @@\n" \
	"+insert at front\n"

/* An insertion at the end of the file (and the resultant patch) */

#define FILE_APPEND \
	"hey!\n" \
	"this is some context!\n" \
	"around some lines\n" \
	"that will change\n" \
	"yes it is!\n" \
	"(this line is changed)\n" \
	"and this\n" \
	"is additional context\n" \
	"below it!\n" \
	"insert at end\n"

#define PATCH_ORIGINAL_TO_APPEND \
	"diff --git a/file.txt b/file.txt\n" \
	"index 9432026..72788bb 100644\n" \
	"--- a/file.txt\n" \
	"+++ b/file.txt\n" \
	"@@ -7,3 +7,4 @@ yes it is!\n" \
	" and this\n" \
	" is additional context\n" \
	" below it!\n" \
	"+insert at end\n"

#define PATCH_ORIGINAL_TO_APPEND_NOCONTEXT \
	"diff --git a/file.txt b/file.txt\n" \
	"index 9432026..72788bb 100644\n" \
	"--- a/file.txt\n" \
	"+++ b/file.txt\n" \
	"@@ -9,0 +10 @@ below it!\n" \
	"+insert at end\n"

/* An insertion at the beginning and end of file (and the resultant patch) */

#define FILE_PREPEND_AND_APPEND \
	"first and\n" \
	"this is some context!\n" \
	"around some lines\n" \
	"that will change\n" \
	"yes it is!\n" \
	"(this line is changed)\n" \
	"and this\n" \
	"is additional context\n" \
	"last lines\n"

#define PATCH_ORIGINAL_TO_PREPEND_AND_APPEND \
	"diff --git a/file.txt b/file.txt\n" \
	"index 9432026..f282430 100644\n" \
	"--- a/file.txt\n" \
	"+++ b/file.txt\n" \
	"@@ -1,4 +1,4 @@\n" \
	"-hey!\n" \
	"+first and\n" \
	" this is some context!\n" \
	" around some lines\n" \
	" that will change\n" \
	"@@ -6,4 +6,4 @@ yes it is!\n" \
	" (this line is changed)\n" \
	" and this\n" \
	" is additional context\n" \
	"-below it!\n" \
	"+last lines\n"

#define PATCH_ORIGINAL_TO_EMPTY_FILE \
	"diff --git a/file.txt b/file.txt\n" \
	"index 9432026..e69de29 100644\n" \
	"--- a/file.txt\n" \
	"+++ b/file.txt\n" \
	"@@ -1,9 +0,0 @@\n" \
	"-hey!\n" \
	"-this is some context!\n" \
	"-around some lines\n" \
	"-that will change\n" \
	"-yes it is!\n" \
	"-(this line is changed)\n" \
	"-and this\n" \
	"-is additional context\n" \
	"-below it!\n"

#define PATCH_EMPTY_FILE_TO_ORIGINAL \
	"diff --git a/file.txt b/file.txt\n" \
	"index e69de29..9432026 100644\n" \
	"--- a/file.txt\n" \
	"+++ b/file.txt\n" \
	"@@ -0,0 +1,9 @@\n" \
	"+hey!\n" \
	"+this is some context!\n" \
	"+around some lines\n" \
	"+that will change\n" \
	"+yes it is!\n" \
	"+(this line is changed)\n" \
	"+and this\n" \
	"+is additional context\n" \
	"+below it!\n"

#define PATCH_ADD_ORIGINAL \
	"diff --git a/file.txt b/file.txt\n" \
	"new file mode 100644\n" \
	"index 0000000..9432026\n" \
	"--- /dev/null\n" \
	"+++ b/file.txt\n" \
	"@@ -0,0 +1,9 @@\n" \
	"+hey!\n" \
	"+this is some context!\n" \
	"+around some lines\n" \
	"+that will change\n" \
	"+yes it is!\n" \
	"+(this line is changed)\n" \
	"+and this\n" \
	"+is additional context\n" \
	"+below it!\n"

#define PATCH_DELETE_ORIGINAL \
	"diff --git a/file.txt b/file.txt\n" \
	"deleted file mode 100644\n" \
	"index 9432026..0000000\n" \
	"--- a/file.txt\n" \
	"+++ /dev/null\n" \
	"@@ -1,9 +0,0 @@\n" \
	"-hey!\n" \
	"-this is some context!\n" \
	"-around some lines\n" \
	"-that will change\n" \
	"-yes it is!\n" \
	"-(this line is changed)\n" \
	"-and this\n" \
	"-is additional context\n" \
	"-below it!\n"

#define PATCH_RENAME_EXACT \
	"diff --git a/file.txt b/newfile.txt\n" \
	"similarity index 100%\n" \
	"rename from file.txt\n" \
	"rename to newfile.txt\n"

#define PATCH_RENAME_SIMILAR \
	"diff --git a/file.txt b/newfile.txt\n" \
	"similarity index 77%\n" \
	"rename from file.txt\n" \
	"rename to newfile.txt\n" \
	"index 9432026..cd8fd12 100644\n" \
	"--- a/file.txt\n" \
	"+++ b/newfile.txt\n" \
	"@@ -3,7 +3,7 @@ this is some context!\n" \
	" around some lines\n" \
	" that will change\n" \
	" yes it is!\n" \
	"-(this line is changed)\n" \
	"+(THIS line is changed!)\n" \
	" and this\n" \
	" is additional context\n" \
	" below it!\n"

#define PATCH_RENAME_EXACT_QUOTEDNAME \
	"diff --git a/file.txt \"b/foo\\\"bar.txt\"\n" \
	"similarity index 100%\n" \
	"rename from file.txt\n" \
	"rename to \"foo\\\"bar.txt\"\n"

#define PATCH_RENAME_SIMILAR_QUOTEDNAME \
	"diff --git a/file.txt \"b/foo\\\"bar.txt\"\n" \
	"similarity index 77%\n" \
	"rename from file.txt\n" \
	"rename to \"foo\\\"bar.txt\"\n" \
	"index 9432026..cd8fd12 100644\n" \
	"--- a/file.txt\n" \
	"+++ \"b/foo\\\"bar.txt\"\n" \
	"@@ -3,7 +3,7 @@ this is some context!\n" \
	" around some lines\n" \
	" that will change\n" \
	" yes it is!\n" \
	"-(this line is changed)\n" \
	"+(THIS line is changed!)\n" \
	" and this\n" \
	" is additional context\n" \
	" below it!\n"

#define PATCH_MODECHANGE_UNCHANGED \
	"diff --git a/file.txt b/file.txt\n" \
	"old mode 100644\n" \
	"new mode 100755\n"

#define PATCH_MODECHANGE_MODIFIED \
	"diff --git a/file.txt b/file.txt\n" \
	"old mode 100644\n" \
	"new mode 100755\n" \
	"index 9432026..cd8fd12\n" \
	"--- a/file.txt\n" \
	"+++ b/file.txt\n" \
	"@@ -3,7 +3,7 @@ this is some context!\n" \
	" around some lines\n" \
	" that will change\n" \
	" yes it is!\n" \
	"-(this line is changed)\n" \
	"+(THIS line is changed!)\n" \
	" and this\n" \
	" is additional context\n" \
	" below it!\n"

#define PATCH_NOISY \
	"This is some\nleading noise\n@@ - that\nlooks like a hunk header\n" \
	"but actually isn't and should parse ok\n" \
	PATCH_ORIGINAL_TO_CHANGE_MIDDLE \
	"plus some trailing garbage for good measure\n"

#define PATCH_NOISY_NOCONTEXT \
	"This is some\nleading noise\n@@ - that\nlooks like a hunk header\n" \
	"but actually isn't and should parse ok\n" \
	PATCH_ORIGINAL_TO_CHANGE_MIDDLE_NOCONTEXT \
	"plus some trailing garbage for good measure\n"

#define PATCH_TRUNCATED_1 \
	"diff --git a/file.txt b/file.txt\n" \
	"index 9432026..cd8fd12 100644\n" \
	"--- a/file.txt\n" \
	"+++ b/file.txt\n" \
	"@@ -3,7 +3,7 @@ this is some context!\n" \
	" around some lines\n" \
	" that will change\n" \
	" yes it is!\n" \
	"-(this line is changed)\n" \
	"+(THIS line is changed!)\n" \
	" and this\n"

#define PATCH_TRUNCATED_2 \
	"diff --git a/file.txt b/file.txt\n" \
	"index 9432026..cd8fd12 100644\n" \
	"--- a/file.txt\n" \
	"+++ b/file.txt\n" \
	"@@ -3,7 +3,7 @@ this is some context!\n" \
	" around some lines\n" \
	"-(this line is changed)\n" \
	"+(THIS line is changed!)\n" \
	" and this\n" \
	" is additional context\n" \
	" below it!\n"

#define PATCH_TRUNCATED_3 \
	"diff --git a/file.txt b/file.txt\n" \
	"index 9432026..cd8fd12 100644\n" \
	"--- a/file.txt\n" \
	"+++ b/file.txt\n" \
	"@@ -3,7 +3,7 @@ this is some context!\n" \
	" around some lines\n" \
	" that will change\n" \
	" yes it is!\n" \
	"+(THIS line is changed!)\n" \
	" and this\n" \
	" is additional context\n" \
	" below it!\n"

#define FILE_EMPTY_CONTEXT_ORIGINAL \
	"this\nhas\nan\n\nempty\ncontext\nline\n"

#define FILE_EMPTY_CONTEXT_MODIFIED \
	"this\nhas\nan\n\nempty...\ncontext\nline\n"

#define PATCH_EMPTY_CONTEXT \
	"diff --git a/file.txt b/file.txt\n" \
	"index 398d2df..bb15234 100644\n" \
	"--- a/file.txt\n" \
	"+++ b/file.txt\n" \
	"@@ -2,6 +2,6 @@ this\n" \
	" has\n" \
	" an\n" \
	"\n" \
	"-empty\n" \
	"+empty...\n" \
	" context\n" \
	" line\n"

#define FILE_APPEND_NO_NL \
	"hey!\n" \
	"this is some context!\n" \
	"around some lines\n" \
	"that will change\n" \
	"yes it is!\n" \
	"(this line is changed)\n" \
	"and this\n" \
	"is additional context\n" \
	"below it!\n" \
	"added line with no nl"

#define PATCH_APPEND_NO_NL \
	"diff --git a/file.txt b/file.txt\n" \
	"index 9432026..83759c0 100644\n" \
	"--- a/file.txt\n" \
	"+++ b/file.txt\n" \
	"@@ -7,3 +7,4 @@ yes it is!\n" \
	" and this\n" \
	" is additional context\n" \
	" below it!\n" \
	"+added line with no nl\n" \
	"\\ No newline at end of file\n"

#define PATCH_CORRUPT_GIT_HEADER \
	"diff --git a/file.txt\n" \
	"index 9432026..0f39b9a 100644\n" \
	"--- a/file.txt\n" \
	"+++ b/file.txt\n" \
	"@@ -0,0 +1 @@\n" \
	"+insert at front\n"

#define PATCH_CORRUPT_MISSING_NEW_FILE \
	"diff --git a/file.txt b/file.txt\n" \
	"index 9432026..cd8fd12 100644\n" \
	"--- a/file.txt\n" \
	"@@ -6 +6 @@ yes it is!\n" \
	"-(this line is changed)\n" \
	"+(THIS line is changed!)\n"

#define PATCH_CORRUPT_MISSING_OLD_FILE \
	"diff --git a/file.txt b/file.txt\n" \
	"index 9432026..cd8fd12 100644\n" \
	"+++ b/file.txt\n" \
	"@@ -6 +6 @@ yes it is!\n" \
	"-(this line is changed)\n" \
	"+(THIS line is changed!)\n"

#define PATCH_CORRUPT_NO_CHANGES \
	"diff --git a/file.txt b/file.txt\n" \
	"index 9432026..cd8fd12 100644\n" \
	"--- a/file.txt\n" \
	"+++ b/file.txt\n" \
	"@@ -0,0 +0,0 @@ yes it is!\n"

#define PATCH_CORRUPT_MISSING_HUNK_HEADER \
	"diff --git a/file.txt b/file.txt\n" \
	"index 9432026..cd8fd12 100644\n" \
	"--- a/file.txt\n" \
	"+++ b/file.txt\n" \
	"-(this line is changed)\n" \
	"+(THIS line is changed!)\n"

#define PATCH_NOT_A_PATCH \
	"+++this is not\n" \
	"--actually even\n" \
	" a legitimate \n" \
	"+patch file\n" \
	"-it's something else\n" \
	" entirely!"
