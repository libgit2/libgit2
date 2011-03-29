#ifndef INCLUDE_tag_h__
#define INCLUDE_tag_h__

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

typedef enum {
	GIT_VAR_INT,
	GIT_VAR_BOOL,
	GIT_VAR_STR
} git_cvar_type;

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
