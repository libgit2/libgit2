#define PATCH_DELETED_FILE_2_HUNKS \
	"diff --git a/a b/a\n" \
	"index 7f129fd..af431f2 100644\n" \
	"--- a/a\n" \
	"+++ b/a\n" \
	"@@ -1 +1 @@\n" \
	"-a contents 2\n" \
	"+a contents\n" \
	"diff --git a/c/d b/c/d\n" \
	"deleted file mode 100644\n" \
	"index 297efb8..0000000\n" \
	"--- a/c/d\n" \
	"+++ /dev/null\n" \
	"@@ -1 +0,0 @@\n" \
	"-c/d contents\n"

#define PATCH_DELETED_FILE_2_HUNKS_SHUFFLED \
	"diff --git a/c/d b/c/d\n" \
	"deleted file mode 100644\n" \
	"index 297efb8..0000000\n" \
	"--- a/c/d\n" \
	"+++ /dev/null\n" \
	"@@ -1 +0,0 @@\n" \
	"-c/d contents\n" \
	"diff --git a/a b/a\n" \
	"index 7f129fd..af431f2 100644\n" \
	"--- a/a\n" \
	"+++ b/a\n" \
	"@@ -1 +1 @@\n" \
	"-a contents 2\n" \
	"+a contents\n"

#define PATCH_ADD_BINARY_NOT_PRINTED \
	"diff --git a/test.bin b/test.bin\n" \
	"new file mode 100644\n" \
	"index 0000000..9e0f96a\n" \
	"Binary files /dev/null and b/test.bin differ\n"

#define PATCH_BINARY_FILE_WITH_TRUNCATED_DELTA \
	"diff --git a/file b/file\n" \
	"index 1420..b71f\n" \
	"GIT binary patch\n" \
	"delta 7\n" \
	"d"
