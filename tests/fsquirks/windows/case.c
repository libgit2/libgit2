/**
 * This test examines case insensitivity on the Windows filesystem.
 * Specifically, how non-US-ASCII characters are handled.  It tries
 * to answer questions about what subset of the full Unicode spec is
 * ACTUALLY IMPLEMENTED by the filesystem.  We brute-force create
 * files to force case collisions and compare them with official
 * Unicode references and with the ignore-case string routines in
 * libgit2.
 *
 * Ideally, when there are discrepancies we want our string routines
 * to BEHAVE MORE LIKE THE FILESYSTEM than the official Unicode spec.
 *
 * Here are some ways to approach the problem:
 * [] Use tolower() and strcasecmp().
 *    Historically, we have use these.
 *    These only handle US-ASCII characters properly.
 *    They were not designed for UTF-8.
 *
 * [] Use wchar_t variants of them.
 *    These either only handle US-ASCII or are locale based.
 *    The specifically exclude the CP_UTF8 pseudo-locale.
 *
 * [] Use an official Unicode library.
 *    At the other end of the spectrum are the ICONV and ICU libraries.
 *    These have ignore-case comparison routines that handle all of
 *    the Unicode characters correctly.
 *    These have a large footprint and add considerable complexity
 *    to the use and redistribution of libgit2.
 *
 *    And they probably DO MORE than the actual filesystem.
 *    {} Type "F" and "T" foldings
 *    {} UTF-16 surrogates / non-BMP characters
 *    {} NFC vs NFD.
 *
 * http://www.unicode.org/Public/UNIDATA/CaseFolding.txt
 *
 * When an NTFS partition is formatted, a case folding table is
 * written to a hidden area on the disk.  This table is constant
 * for the life of the partition.  This table is then used for all
 * ignore-case comparisons for files/folders on that partition.
 * There are no direct APIs to access this table or even determine
 * which version of the table is installed on the partition.
 * See $UpCase:  https://support.microsoft.com/en-us/kb/103657
 *
 * The exact table written to the partition depends on the version
 * of the OS used to format it.  This lets the OS track Unicode
 * standards and adopt new characters as are added to the spec.
 * Therefore, each partition can potentially have a different
 * version of the table.  There are no APIs to even determine
 * which version of the table the system is using.
 *
 * Therefore, it isn't possible to be 100% accurate.  From what
 * I can tell, the CompareStringOrdinal() method in kernel32.dll
 * uses the same table as would installed by the OS into a new
 * partition.  (See also CompareString() and CompareStringEx()
 * in the NLS package.)
 * https://msdn.microsoft.com/en-us/library/windows/desktop/dd317762(v=vs.85).aspx
 *
 * So in this test, if the partition was formatted by the
 * current OS, everything should pass.  If your sandbox is on
 * a partition formatted by an older OS, you may see some
 * issues.  But these should be farily minor (and much closer
 * and easier than using an actual 3rd-party Unicode library).
 *
 */

#include "clar_libgit2.h"
#include "buffer.h"
#include "fileops.h"

#if defined(GIT_WIN32)

static git_win32_path g_utf16_template;
static wchar_t *g_template_pos;  /* start of <x> within template path */
static wchar_t *g_template_base; /* start of filename within template */

static int g_instance = 0;
static const char *g_prefix = "A_";
static const wchar_t g_suffix = L'_';


/**
 * This is defined in Stringapiset.h (via Windows.h) and in kernel32.dll
 * but for some reason we can't link without this prototype here.
 */
WINBASEAPI int WINAPI CompareStringOrdinal(LPCWSTR wcs1, int len1, LPCWSTR wcs2, int len2, BOOL bIgnoreCase);


static bool is_invasive(void)
{
	char *envvar = cl_getenv("GITTEST_INVASIVE_FS_SIZE");
	bool b = (envvar != NULL);
	git__free(envvar);
	return b;
}

/**
 * Create a wchar_t path to serve as a template for the file we try to create
 * and collide.
 * L"<sandbox>/<run>/<prefix>"
 *
 * Actual files will look like L"<sandbox>/<run>/<prefix><x><suffix>"
 */
static void create_template_path(void)
{
	git_buf buf = GIT_BUF_INIT;
	int len;

	git_buf_printf(&buf, "%02d", g_instance++);
	p_mkdir(buf.ptr, 0777);
	git_buf_puts(&buf, "\\");
	git_buf_puts(&buf, g_prefix);

	/* Create a prefix path. */
	cl_assert((len = git_win32_path_from_utf8(g_utf16_template, buf.ptr)) >= 0);
	g_template_pos = &g_utf16_template[len];
	g_template_base = g_template_pos - strlen(g_prefix);

	git_buf_free(&buf);
}

static void my_print(git_buf *buf, const wchar_t *s)
{
	while (*s) {
		if (*s < 0x80)
			git_buf_printf(buf, "%c", *s);
		else
			git_buf_printf(buf, "\\x%04x", *s);
		s++;
	}
}

static void my_print_collision_detail(const char *label, const wchar_t *s1, const wchar_t *s2, bool equal)
{
	git_buf buf = GIT_BUF_INIT;

	git_buf_printf(&buf, "Collision: %s: ", label);
	my_print(&buf, s1);
	git_buf_printf(&buf, " ");
	my_print(&buf, s2);
	git_buf_printf(&buf, " %s", ((equal) ? "equal" : "NOT equal"));

	fprintf(stderr, "%s\n", buf.ptr);

	git_buf_free(&buf);
}

static void my_print_collision(const char *label, const wchar_t *s1, const wchar_t *s2, bool equal)
{
	if (equal) {
		/* my_print_collision_detail(label, s1, s2, TRUE); */
	} else {
		my_print_collision_detail(label, s1, s2, FALSE);
	}
}

static void my_print_error(const char *label, const wchar_t *p)
{
	DWORD dw = GetLastError();
	git_buf buf = GIT_BUF_INIT;
	my_print(&buf, p);

	fprintf(stderr, "%s [%s] -- 0x%08x\n", label, buf.ptr, dw);

	git_buf_free(&buf);
}

/**
 * Confirm that well-known system routine gives same answer as the filesystem.
 */
static int my_equal(const wchar_t *s1, const wchar_t *s2)
{
	int result;
	bool equal;

	result = CompareStringOrdinal(s1, -1, s2, -1, TRUE);
	if (result == 0) {
		my_print_error("Err: CSO failed", s2);
		return 0;
	}

	equal = (result == CSTR_EQUAL);
	my_print_collision("wide", s1, s2, equal);
	return equal;
}

/**
 * Convert the given wchar_t strings to UTF-8 and compare them
 * using the libgit2 ignore-case routine.  It if is using something
 * like tolower() and strcasecmp() this will fail.
 */
static int my_equal_utf8(const wchar_t *s1, const wchar_t *s2)
{
	char utf8_s1[100];
	char utf8_s2[100];
	bool equal;

	if (git__utf16_to_8(utf8_s1, sizeof(utf8_s1), s1) < 0) {
		my_print_error("Err: utf16_to_8 failed", s1);
		return 0;
	}
	if (git__utf16_to_8(utf8_s2, sizeof(utf8_s2), s2) < 0) {
		my_print_error("Err: utf16_to_8 failed", s2);
		return 0;
	}

	equal = (git__strcasecmp(utf8_s1, utf8_s2) == 0);
	my_print_collision("utf8", s1, s2, equal);
	return equal;
}

/**
 * Create a new file using the given pathname.  If we detect that
 * it already exists, then the filesystem caused an aliasing with
 * an earlier pathname.  Read the (original) pathname that we wrote
 * into the file when it was created.  Confirm that our ignore-case
 * routines also consider these 2 pathnames as equivalent.  Finally,
 * append this pathname to the file for use in later collisions.
 */
static void create_one_file(
	const wchar_t *tmpl,
	const wchar_t *tmpl_base,
	int *p_collisions,
	int *p_conflicts_wide,
	int *p_conflicts_utf8)
{
	DWORD bytes_read;
	HANDLE h = NULL;
	wchar_t input[1000];

	/* OpenExisting or Create */
	h = CreateFileW(tmpl, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		my_print_error("Err: CreateFile", tmpl_base);
		goto done;
	}
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		*p_collisions = (*p_collisions) + 1;

		memset(input, 0, sizeof(input));
		if (ReadFile(h, input, sizeof(input), &bytes_read, NULL) == FALSE) {
			my_print_error("Err: Reading file", tmpl_base);
			goto done;
		}

		/* "input" will contain a null-word delimited list of the pathnames
		 * that the filesystem has mapped/folded to this file.  we only use
		 * the first when comparing with the current pathname.
		 */
		if (!my_equal(input, tmpl_base)) {
			*p_conflicts_wide = (*p_conflicts_wide) + 1;
		}
		if (!my_equal_utf8(input, tmpl_base)) {
			*p_conflicts_utf8 = (*p_conflicts_utf8) + 1;
		}

		if (SetFilePointer(h, 0, NULL, FILE_END) == INVALID_SET_FILE_POINTER) {
			my_print_error("Err: SetFilePointer", tmpl_base);
			goto done;
		}
	}

	/* Append the filename we just created / tried to create (plus a trailing null). */
	if (WriteFile(h, tmpl_base, sizeof(wchar_t)*(wcslen(tmpl_base)+1), NULL, NULL) == FALSE) {
		my_print_error("Err: WriteFile", tmpl_base);
		goto done;
	}

done:
	if (h)
		CloseHandle(h);
}


/**
 * Use wchar_t operations to create ALL possible files
 * of the form L"<sandbox>/<prefix><x><suffix>" and write
 * into it the filename we used to create it.
 *
 * If a file already exists (because of a collision) read
 * the previously-written filename and compare it with the
 * filename we just tried to create.  Verify that we think
 * it should be a collision too.
 *
 * Return the number of conflicts.
 */
static int collide_peers_simple(
	const wchar_t *tmpl,
	wchar_t *tmpl_pos,
	const wchar_t *tmpl_base)
{
	int c;
	int c_begin, c_end;
	int collisions = 0;
	int conflicts_wide = 0;
	int conflicts_utf8 = 0;
	BOOL result = FALSE;

	/* Other non-contiguous invalid characters. */
	static wchar_t inv[] = {
		L'"',   /* 0x0022 */
		L'*',   /* 0x002a */
		L':',   /* 0x003a */
		L'/',   /* 0x002f */
		L'<',   /* 0x003c */
		L'>',   /* 0x003e */
		L'?',   /* 0x003f */
		L'\\',  /* 0x005c */
		L'|',   /* 0x007c */
	};
	static int inv_len = sizeof(inv) / sizeof(inv[0]);

	c_begin = 0x0020; /* We skip over the control chars [0x0000..0x001f] because they are not valid in filenames. */
	c_end   = 0xd7ff; /* We stop before the d800..dfff reserved range. */

	for (c=c_begin; c<=c_end; c++) {

		if (c < 0x007f) {
			int k;
			for (k=0; k<inv_len; k++) {
				if (c == inv[k])
					goto skip_char;
			}
		}

		/* update template path to include this candidate character.
		 * add trailing good char to avoid trailing-space and -dot problems.
		 */
		tmpl_pos[0] = (wchar_t)c;
		tmpl_pos[1] = g_suffix;
		tmpl_pos[2] = 0;

		create_one_file(tmpl, tmpl_base, &collisions, &conflicts_wide, &conflicts_utf8);

	skip_char:
		;
	}

	fprintf(stderr, "%s: [collisions %d][wide conflicts %d][utf8 conflicts %d]\n",
			"simple", collisions, conflicts_wide, conflicts_utf8);

	return conflicts_wide + conflicts_utf8;
}

/**
 * The Unicode code points d800..dfff are permantely reserved
 * for UTF-16 surrogate pairs and therefore there are NO VALID
 * characters in this range.  But that doesn't stop someone
 * from creating a file using one of these characters.
 *
 * I single these out in this test so we can later ask if the
 * libgit2 utf8/utf16 conversion routines should throw an error
 * on them.
 */
static int collide_peers_reserved(
	const wchar_t *tmpl,
	wchar_t *tmpl_pos,
	const wchar_t *tmpl_base)
{
	int c;
	int c_begin, c_end;
	int collisions = 0;
	int conflicts_wide = 0;
	int conflicts_utf8 = 0;
	BOOL result = FALSE;

	c_begin = 0xd800;
	c_end   = 0xdfff;

	for (c=c_begin; c<=c_end; c++) {

		/* update template path to include this candidate character.
		 * add trailing good char to avoid trailing-space and -dot problems.
		 */
		tmpl_pos[0] = (wchar_t)c;
		tmpl_pos[1] = g_suffix;
		tmpl_pos[2] = 0;

		create_one_file(tmpl, tmpl_base, &collisions, &conflicts_wide, &conflicts_utf8);
	}

	fprintf(stderr, "%s: [collisions %d][wide conflicts %d][utf8 conflicts %d]\n",
			"reserved", collisions, conflicts_wide, conflicts_utf8);

	return conflicts_wide + conflicts_utf8;
}

/**
 * Unicode code points e000..ffff are labeled as private use.
 */
static int collide_peers_private(
	const wchar_t *tmpl,
	wchar_t *tmpl_pos,
	const wchar_t *tmpl_base)
{
	int c;
	int c_begin, c_end;
	int collisions = 0;
	int conflicts_wide = 0;
	int conflicts_utf8 = 0;
	BOOL result = FALSE;

	c_begin = 0xe000;
	c_end   = 0xffff;

	for (c=c_begin; c<=c_end; c++) {

		/* update template path to include this candidate character.
		 * add trailing good char to avoid trailing-space and -dot problems.
		 */
		tmpl_pos[0] = (wchar_t)c;
		tmpl_pos[1] = g_suffix;
		tmpl_pos[2] = 0;

		create_one_file(tmpl, tmpl_base, &collisions, &conflicts_wide, &conflicts_utf8);
	}

	fprintf(stderr, "%s: [collisions %d][wide conflicts %d][utf8 conflicts %d]\n",
			"private", collisions, conflicts_wide, conflicts_utf8);

	return conflicts_wide + conflicts_utf8;
}


/**
 * Since NTFS uses 16-bit values in pathnames, non-bmp Unicode code points
 * would have to be expressed in UTF-16 surrogate pairs (assuming they are
 * supported).
 *
 * http://www.unicode.org/Public/UNIDATA/CaseFolding.txt
 *
 * Here are the only defined ranges with upper/lower case forms in Unicode 7.0.
 */
struct my_range { unsigned int c_begin, c_end; };
static struct my_range g_my_ranges[] = {
	{ 0x10400, 0x1044f },
	{ 0x118a0, 0x118df }
};
static int g_ranges_len = sizeof(g_my_ranges) / sizeof(g_my_ranges[0]);

/**
 * Determine if NTFS supports case folding for non-bmp code points
 * expressed in UTF-16 surrogate pairs.
 */
static int collide_peers_surrogate(
	const wchar_t *tmpl,
	wchar_t *tmpl_pos,
	const wchar_t *tmpl_base)
{
	int r;
	int collisions = 0;
	int conflicts_wide = 0;
	int conflicts_utf8 = 0;
	BOOL result = FALSE;

	for (r = 0; r < g_ranges_len; r++) {
		unsigned int c;
		unsigned int c_begin = g_my_ranges[r].c_begin;
		unsigned int c_end   = g_my_ranges[r].c_end;

		for (c = c_begin; c <= c_end; c++) {
			unsigned int x = c - 0x10000;
			unsigned int hi = ((x >> 10)   + 0xd800);
			unsigned int lo = ((x & 0x3ff) + 0xdc00);

			tmpl_pos[0] = (wchar_t)hi;
			tmpl_pos[1] = (wchar_t)lo;
			tmpl_pos[2] = g_suffix;
			tmpl_pos[3] = 0;

			create_one_file(tmpl, tmpl_base, &collisions, &conflicts_wide, &conflicts_utf8);
		}
	}

	fprintf(stderr, "%s: [collisions %d][wide conflicts %d][utf8 conflicts %d]\n",
			"surrogate", collisions, conflicts_wide, conflicts_utf8);

	if (collisions == 0) {
		/* NTFS does not case fold non-bmp code points.
		 * This is OK, but since there were no collisions, we haven't tested
		 * that our ignore-case routines also disregard non-bmp code points.
		 * If we start using ICU or ICONV, for example, they might (properly
		 * for Unicode, but not NTFS) respect them.
		 *
		 * TODO Consider testing that we disregard them.
		 */
	}

	return conflicts_wide + conflicts_utf8;
}

#endif /* GIT_WIN32 */

/*****************************************************************/

void test_fsquirks_windows_case__initialize(void)
{
#if defined(GIT_WIN32)
	if (!is_invasive())
		cl_skip();

	create_template_path();

	fflush(stdout);
	fflush(stderr);
#else
	cl_skip();
#endif
}

void test_fsquirks_windows_case__cleanup(void)
{
}

/**
 * Use the "whack whack" version of the wchar_t path.
 * Where the template begins with L"//?/<sandbox>..."
 */
void test_fsquirks_windows_case__whackwhack(void)
{
#if defined(GIT_WIN32)
	const wchar_t *p_tmpl = g_utf16_template;
	int conflicts = 0;

	conflicts += collide_peers_simple(p_tmpl, g_template_pos, g_template_base);
	conflicts += collide_peers_reserved(p_tmpl, g_template_pos, g_template_base);
	conflicts += collide_peers_private(p_tmpl, g_template_pos, g_template_base);
	conflicts += collide_peers_surrogate(p_tmpl, g_template_pos, g_template_base);

	fflush(stdout);
	fflush(stderr);

	cl_assert(conflicts == 0);
#endif
}

/**
 * Do case collision test using a "drive letter" version of the
 * wchar_t path so we use the full Win32 processing.
 * Where the template begins with L"C:/<sandbox>..."
 *
 * Skip the test if the sandbox happens to be on a network share.
 */
void test_fsquirks_windows_case__driveletter(void)
{
#if defined(GIT_WIN32)
	const wchar_t *p_tmpl = g_utf16_template;
	int conflicts = 0;

	if (wcsncmp(p_tmpl, L"\\\\?\\", 4) == 0) {
		p_tmpl += 4;
		if (wcsncmp(p_tmpl, L"UNC\\", 4) == 0) {
			/* I don't want to abuse the share/net. */
			cl_skip();
		}
		cl_assert(p_tmpl[1] == L':');
	}

	conflicts += collide_peers_simple(p_tmpl, g_template_pos, g_template_base);
	conflicts += collide_peers_reserved(p_tmpl, g_template_pos, g_template_base);
	conflicts += collide_peers_private(p_tmpl, g_template_pos, g_template_base);
	conflicts += collide_peers_surrogate(p_tmpl, g_template_pos, g_template_base);

	fflush(stdout);
	fflush(stderr);

	cl_assert(conflicts == 0);
#endif
}
