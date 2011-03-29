#ifndef INCLUDE_config_h__
#define INCLUDE_config_h__

#include "git2/config.h"

struct git_config {
	char *file_path;

	struct {
		gitfo_buf buffer;
		char *read_ptr;
		int line_number;
		int eof;
	} reader;

	git_hashtable *vars;
};

struct git_cvar {
	git_cvar_type type;
	char *name;
	union {
		unsigned char boolean;
		long integer;
		char *string;
	} value;
};

#endif
