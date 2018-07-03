/*
 * libgit2 "submodule" example - shows how to use the submodule API
 *
 * Written by the libgit2 contributors
 *
 * To the extent possible under law, the author(s) have dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 *
 * You should have received a copy of the CC0 Public Domain Dedication along
 * with this software. If not, see
 * <http://creativecommons.org/publicdomain/zero/1.0/>.
 */

#include <stdarg.h>

#include "common.h"

#define UNUSED(x) (void)(x)

enum submodule_cmd {
	SUBMODULE_ADD,
	SUBMODULE_INIT,
	SUBMODULE_SUMMARY,
	SUBMODULE_SYNC,
	SUBMODULE_UPDATE,
};

struct opts {
	git_repository *repo;
	enum submodule_cmd cmd;
	int argc;
	char **argv;

	union {
		struct {
			const char *repository;
			const char *path;
		} add;
		struct {
			unsigned char init;
		} update;
	};
};

static void usage(const char *message, ...)
{
	if (message) {
		va_list ap;
		va_start(ap, message);
		vfprintf(stderr, message, ap);
		va_end(ap);
		fputc('\n', stderr);
	}

	fprintf(stderr, "usage: submodule add <repository> <path>\n");
	fprintf(stderr, "usage: submodule init [<path>...]\n");
	fprintf(stderr, "usage: submodule summary [<path>...]\n");
	fprintf(stderr, "usage: submodule sync [<path>...]\n");
	fprintf(stderr, "usage: submodule update [<path>...]\n");
	exit(1);
}

static void parse_opts(struct opts *opts, int argc, char *argv[])
{
	struct args_info args = ARGS_INFO_INIT;
	const char *arg;

	memset(opts, 0, sizeof(*opts));

	/** We need to have a subcommand */
	if (argc < 2)
		usage("No mode given");

	arg = argv[++args.pos];
	if (!strcmp(arg, "add"))
		opts->cmd = SUBMODULE_ADD;
	else if (!strcmp(arg, "init"))
		opts->cmd = SUBMODULE_INIT;
	else if (!strcmp(arg, "summary"))
		opts->cmd = SUBMODULE_SUMMARY;
	else if (!strcmp(arg, "sync"))
		opts->cmd = SUBMODULE_SYNC;
	else if (!strcmp(arg, "update"))
		opts->cmd = SUBMODULE_UPDATE;
	else
		usage("Invalid subcommand '%s'", arg);

	for (args.pos++ ; args.pos < args.argc; args.pos++) {
		arg = argv[args.pos];

		switch (opts->cmd) {
		case SUBMODULE_ADD:
			if (opts->add.repository == NULL)
				opts->add.repository = arg;
			else if (opts->add.path == NULL)
				opts->add.path = arg;
			else
				usage(NULL);
			break;
		case SUBMODULE_UPDATE:
			if (!strcmp(arg, "--init"))
				opts->update.init = 1;
			break;
		case SUBMODULE_SUMMARY:
		case SUBMODULE_INIT:
		case SUBMODULE_SYNC:
			goto finish;
		}
	}

	/**
	 * 'submodule add' is the only command that that has a
	 * required parameter.
	 */
	if (opts->cmd == SUBMODULE_ADD && !opts->add.repository && !opts->add.path)
		usage(NULL);

finish:
	/**
	 * Set up the remainder of the arguments. In case there
	 * are any more arguments, they will be treated as the
	 * submodule paths that should be handled.
	 */
	opts->argc = argc - args.pos;
	opts->argv = &argv[args.pos];
}

/**
 * This function is a helper function to ease iterating over the
 * submodules. All submodule commands except 'add' either iterate
 * over all submodules in case no additional arguments are given,
 * or otherwise over the list of given submodule paths. This
 * function here abstracts that to make it reusable for all
 * commands.
 */
static void iterate_submodules(struct opts *opts, git_submodule_cb cb)
{
	int i;

	if (!opts->argc) {
		/**
		 * This will loop over all submodules and call
		 * `cb` for each of them.
		 */
		check_lg2(git_submodule_foreach(opts->repo, cb, opts),
			"Could not loop over submodules", NULL);
		return;
	}

	for (i = 0; i < opts->argc; i++) {
		struct git_submodule *sm;
		/** Look up the submodule by path and pass it to `cb`. */
		check_lg2(git_submodule_lookup(&sm, opts->repo, opts->argv[i]),
			"Could not lookup submodule", NULL);
		check_lg2(cb(sm, git_submodule_name(sm), opts),
			"Could not update submodule", NULL);
		git_submodule_free(sm);
	}
}

static void submodule_add(struct opts *opts)
{
	git_submodule *sm;

	/**
	 * Create the submodule configuration and add it to the
	 * .gitmodules file. This function will also create the
	 * initial empty submodule repository and seed its
	 * configuration.
	 */
	check_lg2(git_submodule_add_setup(&sm, opts->repo,
		    opts->add.repository, opts->add.path, 1),
		"Could not add submodule", NULL);

	/**
	 * Clone the added submodule.
	 */
	check_lg2(git_submodule_clone(NULL, sm, NULL),
		"Could not clone submodule", NULL);

	/**
	 * Finalize the submodule configuration. This will
	 * complete the setup of the new submodule by adding the
	 * .gitmodules file and the submodule entry in the
	 * working directory to the index.
	 */
	check_lg2(git_submodule_add_finalize(sm),
		"Could not finalize submodule setup", NULL);

	git_submodule_free(sm);
}

static int submodule_init(git_submodule *sm, const char *name, void *payload)
{
	UNUSED(payload);

	/**
	 * Initialize the submodule. We have the `overwrite`
	 * parameter set to `0` such that existing configuration
	 * will not be forcibly overridden in case a submodule
	 * has already been initialized before.
	 */
	check_lg2(git_submodule_init(sm, 0),
		"Unable to initialize submodule", NULL);

	printf("Submodule '%s' (%s) registered for path '%s'\n",
		name, git_submodule_url(sm), git_submodule_path(sm));

	return 0;
}

static int submodule_summary(git_submodule *sm, const char *name, void *payload)
{
	const git_oid *head;
	int initialized = 0;
	UNUSED(payload);

	/**
	 * We need to determine whether the submodule is
	 * initialized in the working directory. In case a
	 * submodule is not initialized, git will print out a '-'
	 * before printing the actual submodule name.
	 *
	 * Also, we need to get the currently checked out
	 * submodule OID. In case the submodule is initialized
	 * and checked out, we will take the commit that is
	 * currently checked out in the submodule working
	 * directory. In case it is not initialized, we will take
	 * the OID that is recorded in the HEAD commit in the
	 * .gitmodules file.
	 */
	if ((head = git_submodule_wd_id(sm)) != NULL)
		initialized = 1;
	else
	    head = git_submodule_head_id(sm);

	printf("%c%s %s\n", initialized ? ' ' : '-',
		git_oid_tostr_s(head), name);

	return 0;
}

static int submodule_sync(git_submodule *sm, const char *name, void *payload)
{
	UNUSED(payload);
	printf("Synchronizing submodule url for '%s'\n", name);

	/**
	 * This will update the submodule configuration in
	 * 'superrepo/.git/config' to have the same values as in
	 * the '.gitmodules' file.
	 */
	check_lg2(git_submodule_sync(sm),
		"Failed to synchronize submodule", NULL);

	return 0;
}

static int submodule_update(git_submodule *sm, const char *name, void *payload)
{
	struct opts *opts = (struct opts *) payload;

	/**
	 * Calling `git_submodule_update` will update the
	 * submodule so that its repository will point to the
	 * commit recorded in the submodule configuration.
	 * In case `opts->update.init` is set, the function will
	 * also initialize the submodule in case it wasn't yet
	 * initialized.
	 */
	check_lg2(git_submodule_update(sm, opts->update.init, NULL),
		"Failed to update submodule", NULL);

	printf("Submodule path '%s': checked out '%s'\n",
		name, git_oid_tostr_s(git_submodule_head_id(sm)));

	return 0;
}

int lg2_submodule(git_repository *repo, int argc, char **argv)
{
	struct opts opts;

	parse_opts(&opts, argc, argv);
	opts.repo = repo;

	switch (opts.cmd) {
	case SUBMODULE_ADD:
		submodule_add(&opts);
		break;
	case SUBMODULE_INIT:
		iterate_submodules(&opts, submodule_init);
		break;
	case SUBMODULE_SUMMARY:
		iterate_submodules(&opts, submodule_summary);
		break;
	case SUBMODULE_SYNC:
		iterate_submodules(&opts, submodule_sync);
		break;
	case SUBMODULE_UPDATE:
		iterate_submodules(&opts, submodule_update);
		break;
	}

	return 0;
}
