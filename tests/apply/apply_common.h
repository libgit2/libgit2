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
