#ifndef CLI_printerr_h__
#define CLI_printerr_h__

extern void cli_error(const char *fmt, ...);
extern void cli_error_git(const char *fmt, ...);
extern void cli_error_os(const char *fmt, ...);

#define cli_die(...) \
	do { cli_error(__VA_ARGS__); exit(1); } while(0)
#define cli_die_git(...) \
	do { cli_error_git(__VA_ARGS__); exit(1); } while(0)
#define cli_die_os(...) \
	do { cli_error_os(__VA_ARGS__); exit(1); } while(0)

#endif /* CLI_printerr_h__ */
