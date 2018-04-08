#include "mailmap.h"

typedef struct mailmap_entry {
	const char *real_name;
	const char *real_email;
	const char *replace_name;
	const char *replace_email;
} mailmap_entry;

static const char string_mailmap[] =
	"# Simple Comment line\n"
	"<cto@company.xx>                       <cto@coompany.xx>\n"
	"Some Dude <some@dude.xx>         nick1 <bugs@company.xx>\n"
	"Other Author <other@author.xx>   nick2 <bugs@company.xx>\n"
	"Other Author <other@author.xx>         <nick2@company.xx>\n"
	"Phil Hill <phil@company.xx>  # Comment at end of line\n"
	"<joseph@company.xx>             Joseph <bugs@company.xx>\n"
	"Santa Claus <santa.claus@northpole.xx> <me@company.xx>\n"
	"Untracked <untracked@company.xx>";

static const mailmap_entry entries[] = {
	{ NULL, "cto@company.xx", NULL, "cto@coompany.xx" },
	{ "Some Dude", "some@dude.xx", "nick1", "bugs@company.xx" },
	{ "Other Author", "other@author.xx", "nick2", "bugs@company.xx" },
	{ "Other Author", "other@author.xx", NULL, "nick2@company.xx" },
	{ "Phil Hill", NULL, NULL, "phil@company.xx" },
	{ NULL, "joseph@company.xx", "Joseph", "bugs@company.xx" },
	{ "Santa Claus", "santa.claus@northpole.xx", NULL, "me@company.xx" },
	/* This entry isn't in the bare repository */
	{ "Untracked", NULL, NULL, "untracked@company.xx" }
};

static const mailmap_entry resolved[] = {
	{ "Brad", "cto@company.xx", "Brad", "cto@coompany.xx" },
	{ "Brad L", "cto@company.xx", "Brad L", "cto@coompany.xx" },
	{ "Some Dude", "some@dude.xx", "nick1", "bugs@company.xx" },
	{ "Other Author", "other@author.xx", "nick2", "bugs@company.xx" },
	{ "nick3", "bugs@company.xx", "nick3", "bugs@company.xx" },
	{ "Other Author", "other@author.xx", "Some Garbage", "nick2@company.xx" },
	{ "Phil Hill", "phil@company.xx", "unknown", "phil@company.xx" },
	{ "Joseph", "joseph@company.xx", "Joseph", "bugs@company.xx" },
	{ "Santa Claus", "santa.claus@northpole.xx", "Clause", "me@company.xx" },
	{ "Charles", "charles@charles.xx", "Charles", "charles@charles.xx" }
};
