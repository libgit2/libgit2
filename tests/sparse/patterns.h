#include "util.h"

const char *default__patterns[] = { "/*", "!/*/" };
const git_strarray g_default_patterns = { default__patterns, ARRAY_SIZE(default__patterns) };
