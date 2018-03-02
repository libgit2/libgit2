/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */
#ifndef INCLUDE_userdiff_h__
#define INCLUDE_userdiff_h__

/*
 * This file isolates the built in diff driver function name patterns.
 * Most of these patterns are taken from Git (with permission from the
 * original authors for relicensing to libgit2).
 */

typedef struct {
	const char *name;
	const char *fns;
	const char *words;
	int flags;
} git_diff_driver_definition;

#define WORD_DEFAULT "|[^[:space:]]"

/*
 * These builtin driver definition macros have same signature as in core
 * git userdiff.c so that the data can be extracted verbatim
 */
#define PATTERNS(NAME, FN_PATS, WORD_PAT) \
	{ NAME, FN_PATS, WORD_PAT WORD_DEFAULT, 0 }
#define IPATTERN(NAME, FN_PATS, WORD_PAT) \
	{ NAME, FN_PATS, WORD_PAT WORD_DEFAULT, REG_ICASE }

/*
 * The table of diff driver patterns
 *
 * Function name patterns are a list of newline separated patterns that
 * match a function declaration (i.e. the line you want in the hunk header),
 * or a negative pattern prefixed with a '!' to reject a pattern (such as
 * rejecting goto labels in C code).
 *
 * Word boundary patterns are just a simple pattern that will be OR'ed with
 * the default value above (i.e. whitespace or non-ASCII characters).
 */
static git_diff_driver_definition builtin_defs[] = {

IPATTERN("ada",
	 "!^(.*[ \t])?(is[ \t]+new|renames|is[ \t]+separate)([ \t].*)?$\n"
	 "!^[ \t]*with[ \t].*$\n"
	 "^[ \t]*((procedure|function)[ \t]+.*)$\n"
	 "^[ \t]*((package|protected|task)[ \t]+.*)$",
	 /* -- */
	 "[[:alpha:]][[:alnum:]_]*"
	 "|[-+]?[[:digit:]][[:xdigit:]#_.]*([eE][+-]?[[:digit:]_]+)?"
	 "|=>|\\.\\.|\\*\\*|:=|/=|>=|<=|<<|>>|<>"),

IPATTERN("fortran",
	 "!^([C*]|[ \t]*!)\n"
	 "!^[ \t]*MODULE[ \t]+PROCEDURE[ \t]\n"
	 "^[ \t]*((END[ \t]+)?(PROGRAM|MODULE|BLOCK[ \t]+DATA"
		"|([^'\" \t]+[ \t]+)*(SUBROUTINE|FUNCTION))[ \t]+[[:upper:]].*)$",
	 /* -- */
	 "[[:alpha:]][[:alnum:]_]*"
	 "|\\.([Ee][Qq]|[Nn][Ee]|[Gg][TtEe]|[Ll][TtEe]|[Tt][Rr][Uu][Ee]|[Ff][Aa][Ll][Ss][Ee]|[Aa][Nn][Dd]|[Oo][Rr]|[Nn]?[Ee][Qq][Vv]|[Nn][Oo][Tt])\\."
	 /* numbers and format statements like 2E14.4, or ES12.6, 9X.
	  * Don't worry about format statements without leading digits since
	  * they would have been matched above as a variable anyway. */
	 "|[-+]?[[:digit:].]+([AaIiDdEeFfLlTtXx][Ss]?[-+]?[[:digit:].]*)?(_[[:alnum:]][[:alnum:]_]*)?"
	 "|//|\\*\\*|::|[/<>=]="),

PATTERNS("html", "^[ \t]*(<[Hh][1-6][ \t].*>.*)$",
	 "[^<>= \t]+"),

PATTERNS("java",
	 "!^[ \t]*(catch|do|for|if|instanceof|new|return|switch|throw|while)\n"
	 "^[ \t]*(([[:alpha:]_][[:alnum:]_]*[ \t]+)+[[:alpha:]_][[:alnum:]_]*[ \t]*\\([^;]*)$",
	 /* -- */
	 "[[:alpha:]_][[:alnum:]_]*"
	 "|[-+[:digit:].e]+[fFlL]?|0[xXbB]?[[:xdigit:]]+[lL]?"
	 "|[-+*/<>%&^|=!]="
	 "|--|\\+\\+|<<=?|>>>?=?|&&|\\|\\|"),

PATTERNS("matlab",
	 "^[[:space:]]*((classdef|function)[[:space:]].*)$|^%%[[:space:]].*$",
	 "[[:alpha:]_][[:alnum:]_]*|[-+[[:digit:]].e]+|[=~<>]=|\\.[*/\\^']|\\|\\||&&"),

PATTERNS("objc",
	 /* Negate C statements that can look like functions */
	 "!^[ \t]*(do|for|if|else|return|switch|while)\n"
	 /* Objective-C methods */
	 "^[ \t]*([-+][ \t]*\\([ \t]*[[:alpha:]_][[:alnum:]_* \t]*\\)[ \t]*[[:alpha:]z_].*)$\n"
	 /* C functions */
	 "^[ \t]*(([[:alpha:]_][[:alnum]_]*[ \t]+)+[[:alpha]_][[:alnum:]_]*[ \t]*\\([^;]*)$\n"
	 /* Objective-C class/protocol definitions */
	 "^(@(implementation|interface|protocol)[ \t].*)$",
	 /* -- */
	 "[[:alpha:]_][[:alnum:]_]*"
	 "|[-+[[:digit:]].e]+[fFlL]?|0[xXbB]?[[:xdigit:]]+[lL]?"
	 "|[-+*/<>%&^|=!]=|--|\\+\\+|<<=?|>>=?|&&|\\|\\||::|->"),

PATTERNS("pascal",
	 "^(((class[ \t]+)?(procedure|function)|constructor|destructor|interface|"
		"implementation|initialization|finalization)[ \t]*.*)$"
	 "\n"
	 "^(.*=[ \t]*(class|record).*)$",
	 /* -- */
	 "[[:alpha:]_][[:alpha:]_]*"
	 "|[-+[[:digit:]].e]+|0[xXbB]?[[:xdigit:]]+"
	 "|<>|<=|>=|:=|\\.\\."),

PATTERNS("perl",
	 "^package .*\n"
	 "^sub [[:alnum:]_':]+[ \t]*"
		"(\\([^)]*\\)[ \t]*)?" /* prototype */
		/*
		 * Attributes.  A regex can't count nested parentheses,
		 * so just slurp up whatever we see, taking care not
		 * to accept lines like "sub foo; # defined elsewhere".
		 *
		 * An attribute could contain a semicolon, but at that
		 * point it seems reasonable enough to give up.
		 */
		"(:[^;#]*)?"
		"(\\{[ \t]*)?" /* brace can come here or on the next line */
		"(#.*)?$\n" /* comment */
	 "^(BEGIN|END|INIT|CHECK|UNITCHECK|AUTOLOAD|DESTROY)[ \t]*"
		"(\\{[ \t]*)?" /* brace can come here or on the next line */
		"(#.*)?$\n"
	 "^=head[[:digit:]] .*",	/* POD */
	 /* -- */
	 "[[:alpha:]_'][[:alnum:]_']*"
	 "|0[xb]?[[:xdigit:]_]*"
	 /* taking care not to interpret 3..5 as (3.)(.5) */
	 "|[[:xdigit:]_]+(\\.[[:xdigit:]_]+)?([eE][-+]?[[:digit:]_]+)?"
	 "|=>|-[rwxoRWXOezsfdlpSugkbctTBMAC>]|~~|::"
	 "|&&=|\\|\\|=|//=|\\*\\*="
	 "|&&|\\|\\||//|\\+\\+|--|\\*\\*|\\.\\.\\.?"
	 "|[-+*/%.^&<>=!|]="
	 "|=~|!~"
	 "|<<|<>|<=>|>>"),

PATTERNS("python", "^[ \t]*((class|def)[ \t].*)$",
	 /* -- */
	 "[[:alpha:]_][[:alnum:]_]*"
	 "|[-+[[:digit:]].e]+[jJlL]?|0[xX]?[[:xdigit:]]+[lL]?"
	 "|[-+*/<>%&^|=!]=|//=?|<<=?|>>=?|\\*\\*=?"),

PATTERNS("ruby", "^[ \t]*((class|module|def)[ \t].*)$",
	 /* -- */
	 "(@|@@|\\$)?[[:alpha:]_][[:alnum:]_]*"
	 "|[-+[[:digit:]].e]+|0[xXbB]?[[:xdigit:]]+|\\?(\\\\C-)?(\\\\M-)?."
	 "|//=?|[-+*/<>%&^|=!]=|<<=?|>>=?|===|\\.{1,3}|::|[!=]~"),

PATTERNS("bibtex", "(@[[:alpha:]]{1,}[ \t]*\\{{0,1}[ \t]*[^ \t\"@',\\#}{~%]*).*$",
	 "[={}\"]|[^={}\" \t]+"),

PATTERNS("tex", "^(\\\\((sub)*section|chapter|part)\\*{0,1}\\{.*)$",
	 "\\\\[[:alpha:]@]+|\\\\.|[[:alnum:]\x80-\xff]+"),

PATTERNS("cpp",
	 /* Jump targets or access declarations */
	 "!^[ \t]*[[:alpha:]_][[:alnum:]_]*:[[:space:]]*($|/[/*])\n"
	 /* functions/methods, variables, and compounds at top level */
	 "^((::[[:space:]]*)?[[:alpha:]_].*)$",
	 /* -- */
	 "[[:alpha:]_][[:alnum:]_]*"
	 "|[-+[[:digit:]].e]+[fFlL]?|0[xXbB]?[[:xdigit:]]+[lLuU]*"
	 "|[-+*/<>%&^|=!]=|--|\\+\\+|<<=?|>>=?|&&|\\|\\||::|->\\*?|\\.\\*"),

PATTERNS("csharp",
	 /* Keywords */
	 "!^[ \t]*(do|while|for|if|else|instanceof|new|return|switch|case|throw|catch|using)\n"
	 /* Methods and constructors */
	 "^[ \t]*(((static|public|internal|private|protected|new|virtual|sealed|override|unsafe)[ \t]+)*[][<>@.~_[:alnum:]]+[ \t]+[<>@._[:alnum:]]+[ \t]*\\(.*\\))[ \t]*$\n"
	 /* Properties */
	 "^[ \t]*(((static|public|internal|private|protected|new|virtual|sealed|override|unsafe)[ \t]+)*[][<>@.~_[:alnum:]]+[ \t]+[@._[:alnum:]]+)[ \t]*$\n"
	 /* Type definitions */
	 "^[ \t]*(((static|public|internal|private|protected|new|unsafe|sealed|abstract|partial)[ \t]+)*(class|enum|interface|struct)[ \t]+.*)$\n"
	 /* Namespace */
	 "^[ \t]*(namespace[ \t]+.*)$",
	 /* -- */
	 "[[:alpha:]_][[:alnum:]_]*"
	 "|[-+[[:digit:]].e]+[fFlL]?|0[xXbB]?[[:xdigit:]]+[lL]?"
	 "|[-+*/<>%&^|=!]=|--|\\+\\+|<<=?|>>=?|&&|\\|\\||::|->"),

PATTERNS("php",
	 "^[ \t]*(((public|private|protected|static|final)[ \t]+)*((class|function)[ \t].*))$",
	 /* -- */
	 "[[:alpha:]_][[:alnum:]_]*"
	 "|[-+[[:digit:]].e]+[fFlL]?|0[xX]?[[:xdigit:]]+[lL]?"
	 "|[-+*/<>%&^|=!]=|--|\\+\\+|<<=?|>>=?|&&|\\|\\||::|->"),

PATTERNS("javascript",
	 "([[:alpha:]_$][[:alnum:]_$]*(\\.[[:alnum:]_$]+)*[ \t]*=[ \t]*function([ \t][[:alpha:]_$][[:alnum:]_$]*)?[^\\{]*)\n"
	 "([[:alpha:]_$][[:alnum:]_$]*[ \t]*:[ \t]*function([ \t][[:alpha:]_$][[:alnum:]_$]*)?[^\\{]*)\n"
	 "[^[:alnum:]_\\$](function([ \t][[:alpha:]_$][[:alnum:]_$]*)?[^\\{]*)",
	 /* -- */
	 "[[:alpha:]_][[:alnum:]_]*"
	 "|[-+[[:digit:]].e]+[fFlL]?|0[xX]?[[:xdigit:]]+[lL]?"
	 "|[-+*/<>%&^|=!]=|--|\\+\\+|<<=?|>>=?|&&|\\|\\||::|->"),
};

#undef IPATTERN
#undef PATTERNS
#undef WORD_DEFAULT

#endif

