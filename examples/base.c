/*
 * Utilities library for libgit2 examples
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <git2.h>

#define GIT_AUTHOR_NAME_ENVIRONMENT "GIT_AUTHOR_NAME"
#define GIT_AUTHOR_EMAIL_ENVIRONMENT "GIT_AUTHOR_EMAIL"
#define GIT_AUTHOR_DATE_ENVIRONMENT "GIT_AUTHOR_DATE"

#define GIT_COMMITTER_NAME_ENVIRONMENT "GIT_COMMITTER_NAME"
#define GIT_COMMITTER_EMAIL_ENVIRONMENT "GIT_COMMITTER_EMAIL"
#define GIT_COMMITTER_DATE_ENVIRONMENT "GIT_COMMITTER_DATE"

#define LOCALGIT "DemoGit"
#define ORIGINURL "https://github.com/xiangism/DemoGit.git"

static int cred_acquire_cb(git_cred **out,
                    const char * url,
                    const char * username_from_url,
                    unsigned int allowed_types,
                    void *payload)
{
    char username[128] = {0};
    char password[128] = {0};

#include "../password.txt"
    //The context of password.txt like this:
    //sprintf(username, "%s", "account");
    //sprintf(password, "%s", "password");

    return git_cred_userpass_plaintext_new(out, username, password);
}

void check_error()
{
    const git_error *error = giterr_last();
    if (error)
    {
        printf("Error %d - %s\n", error->klass, error->message ? error->message : "???");
    }

}

/*implements the function of "git init"*/
void cmd_init()
{
    git_repository *repo = NULL;

	git_libgit2_init();
    git_repository_init(&repo, ".", 0);

	git_repository_free(repo);
	git_libgit2_shutdown();
}

typedef struct progress_data {
    git_transfer_progress fetch_progress;
    size_t completed_steps;
    size_t total_steps;
    const char *path;
} progress_data;

static void checkout_progress(const char *path, size_t cur, size_t tot, void *payload)
{
    progress_data *pd = (progress_data*)payload;
    pd->completed_steps = cur;
    pd->total_steps = tot;
    pd->path = path;
    printf("%d, %d\n", cur, tot);
}

static int fetch_progress(const git_transfer_progress *stats, void *payload)
{
    progress_data *pd = (progress_data*)payload;
    pd->fetch_progress = *stats;
    printf("%d, %d\n", pd->completed_steps, pd->total_steps);
    return 0;
}

/*implements the function of "git clone"*/
void cmd_clone()
{
    progress_data pd = {{0}};
    git_repository *repo = NULL;
    git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
    git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;

    checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
    checkout_opts.progress_cb = checkout_progress;
    checkout_opts.progress_payload = &pd;
    clone_opts.checkout_opts = checkout_opts;
    //clone_opts.fetch_opts.callbacks.sideband_progress = sideband_progress;
    clone_opts.fetch_opts.callbacks.transfer_progress = &fetch_progress;
    clone_opts.fetch_opts.callbacks.credentials = cred_acquire_cb;
    clone_opts.fetch_opts.callbacks.payload = &pd;

    git_libgit2_init();

    git_clone(&repo, ORIGINURL, LOCALGIT, &clone_opts);

    git_repository_free(repo);
    git_libgit2_shutdown();
}


enum print_options {
    SKIP = 1,
    VERBOSE = 2,
    UPDATE = 4,
};

typedef struct print_payload {
    enum print_options options;
    git_repository *repo;
}print_payload;

struct mergehead_peel_payload
{
    git_repository *repo;
    git_commit **next_parent;
};

static int mergehead_peel_cb(const git_oid *oid, void *payload)
{
	int err;
	struct mergehead_peel_payload *peel_payload = (struct mergehead_peel_payload*)payload;

	if ((err = git_commit_lookup(peel_payload->next_parent, peel_payload->repo, oid)))
		return -1;
	peel_payload->next_parent++;
	return 0;
}

/*implements the function of "git add ."*/
void cmd_add()
{
    git_repository *repo = NULL;
    int count;
    struct git_strarray arr = {0};
    git_index* index;
    print_payload payload;

    git_libgit2_init();
    git_repository_open(&repo, LOCALGIT);

    git_repository_index(&index, repo);

    git_index_add_all(index, &arr, 0, NULL, &payload);
    git_index_write(index);

    check_error();

    git_index_free(index);
    git_repository_free(repo);
    git_libgit2_shutdown();
}


static int mergehead_count_cb(const git_oid *oid, void *payload)
{
    int *nparents = (int*)payload;
    *nparents = *nparents + 1;
    return 0;
}

int sgit_get_author_signature(git_repository *repo, git_signature **author_signature)
{
	int err;
	int author_offset = 0;
	char *author_name = NULL;
	char *author_email = NULL;
	char *author_date = NULL;
	unsigned long author_timestamp = 0;

	err = git_signature_default(author_signature, repo);
	if (err == 0)
		return 0;
	if (err != GIT_ENOTFOUND)
		return err;

	author_name = getenv(GIT_AUTHOR_NAME_ENVIRONMENT);
	author_email = getenv(GIT_AUTHOR_EMAIL_ENVIRONMENT);
	author_date = getenv(GIT_AUTHOR_DATE_ENVIRONMENT);

	if (!author_name || !author_email)
		fprintf(stderr,"Author information not properly configured!\n");

	if (!author_name)
		author_name = "xiangism";

	if (!author_email)
		author_email = "327340773@qq.com";

	/*if (author_date)
	{
		if ((parse_date_basic(author_date, &author_timestamp, &author_offset)))
		{
			fprintf(stderr, "Bad author date format\n!");
			return GIT_ERROR;
		}
	}*/

	if (!author_timestamp)
	{
		return git_signature_now(author_signature, author_name, author_email);
	} else
	{
		return git_signature_new(author_signature, author_name, author_email,
			author_timestamp, author_offset);
	}
}

int sgit_get_committer_signature(git_repository *repo, git_signature** committer_signature)
{
	int err;
	char *committer_name = NULL;
	char *committer_email = NULL;
	char *committer_date = NULL;
	unsigned long committer_timestamp = 0;
	int committer_offset = 0;

	err = git_signature_default(committer_signature, repo);
	if (err == 0)
		return 0;
	if (err != GIT_ENOTFOUND)
		return err;

	committer_name = getenv(GIT_COMMITTER_NAME_ENVIRONMENT);
	committer_email = getenv(GIT_COMMITTER_EMAIL_ENVIRONMENT);
	committer_date = getenv(GIT_COMMITTER_DATE_ENVIRONMENT);

	if (!committer_name || !committer_email)
		fprintf(stderr,"Committer information not properly configured!\n");

	if (!committer_name)
		committer_name = "Dummy Committer";

	if (!committer_email)
		committer_email = "dummyc@dummydummydummy.zz";

	/*if (committer_date)
	{
		if ((parse_date_basic(committer_date, &committer_timestamp, &committer_offset)))
		{
			fprintf(stderr, "Bad committer date format\n!");
			return GIT_ERROR;
		}
	}*/
	if (!committer_timestamp)
	{
		return git_signature_now(committer_signature, committer_name, committer_email);
	} else
	{
		return git_signature_new(committer_signature, committer_name,
				committer_email, committer_timestamp, committer_offset);
	}
}
int sgit_repository_mergeheads_count(int *num_parents, git_repository *repo)
{
    return git_repository_mergehead_foreach(repo, mergehead_count_cb, num_parents);
}


struct dl_data {
    git_remote *remote;
    int ret;
    int finished;
};

static int push_update_reference_callback(const char *refname, const char *status, void *data)
{
	printf("%s: %s\n", refname, status?status:"Ok");
	return 0;
}

int cmd_push_repo(git_repository *repo)
{
	int err = GIT_OK;
	int rc = EXIT_FAILURE;
	git_push_options opts = GIT_PUSH_OPTIONS_INIT;
	char *ref_fullname = NULL;

	git_remote *remote = NULL;

    char *name = "origin";
    char *name2 = "refs/heads/master";

    const git_strarray refs = {&name2, 1};
    git_reference *ref;
    git_push_update expected;

    if ((err = git_remote_lookup(&remote, repo, name)) != GIT_OK)
        goto out;

    git_reference_lookup(&ref, repo, "refs/heads/master");

    expected.src_refname = "";
    expected.dst_refname = "refs/heads/master";
    memset(&expected.dst, 0, sizeof(git_oid));
    git_oid_cpy(&expected.src, git_reference_target(ref));

	opts.callbacks.push_update_reference = push_update_reference_callback;
    opts.callbacks.payload = &expected;
	opts.callbacks.credentials = cred_acquire_cb;

	if ((err = git_remote_push(remote, &refs, &opts)))
		goto out;

out:
    check_error();
	free(ref_fullname);
	if (remote) git_remote_free(remote);
	return rc;
}

/*implements the function of "git push"*/
void cmd_push()
{
    git_repository *repo = NULL;

    git_libgit2_init();
    git_repository_open(&repo, LOCALGIT);

    cmd_push_repo(repo);

    git_repository_free(repo);
    git_libgit2_shutdown();
}


static void cmd_remote_list()
{
    git_repository *repo = NULL;
    git_strarray remotes = {0};
    int i;

    git_libgit2_init();
    git_repository_open(&repo, LOCALGIT);

    git_remote_list(&remotes, repo);
    for (i = 0; i < (int) remotes.count; i++) {
        const char *name = remotes.strings[i];
        continue;
    }

    git_repository_free(repo);
    git_libgit2_shutdown();
}

static int update_cb(const char *refname, const git_oid *a, const git_oid *b, void *data)
{
    char a_str[GIT_OID_HEXSZ+1], b_str[GIT_OID_HEXSZ+1];
    (void)data;

    git_oid_fmt(b_str, b);
    b_str[GIT_OID_HEXSZ] = '\0';

    if (git_oid_iszero(a)) {
        printf("[new]     %.20s %s\n", b_str, refname);
    } else {
        git_oid_fmt(a_str, a);
        a_str[GIT_OID_HEXSZ] = '\0';
        printf("[updated] %.10s..%.10s %s\n", a_str, b_str, refname);
    }

    return 0;
}

static int progress_cb(const char *str, int len, void *data)
{
    (void)data;
    printf("remote: %.*s", len, str);
    fflush(stdout); /* We don't have the \n to force the flush */
    return 0;
}

static int transfer_progress_cb(const git_transfer_progress *stats, void *payload)
{
    if (stats->received_objects == stats->total_objects) {
        printf("Resolving deltas %d/%d\r",
            stats->indexed_deltas, stats->total_deltas);
    } else if (stats->total_objects > 0) {
        //printf("Received %d/%d objects (%d) in %" PRIuZ " bytes\r",
        //stats->received_objects, stats->total_objects,
        //stats->indexed_objects, stats->received_bytes);
    }
    return 0;
}

/*implements the function of "git fetch"*/
void cmd_fetch()
{
    git_repository *repo = NULL;

    git_libgit2_init();
    git_repository_open(&repo, LOCALGIT);

    {
        git_remote *remote = NULL;
        const git_transfer_progress *stats;
        struct dl_data data;
        git_fetch_options fetch_opts = GIT_FETCH_OPTIONS_INIT;
        const char *name = "origin";

        int r = git_remote_lookup(&remote, repo, name);

        fetch_opts.callbacks.update_tips = &update_cb;
        fetch_opts.callbacks.sideband_progress = &progress_cb;
        fetch_opts.callbacks.transfer_progress = transfer_progress_cb;
        fetch_opts.callbacks.credentials = cred_acquire_cb;

        r = git_remote_fetch(remote, NULL, &fetch_opts, "fetch");

        check_error();
        
        stats = git_remote_stats(remote);
        git_remote_free(remote);
    }

    git_repository_free(repo);
    git_libgit2_shutdown();
}

int cmd_merge_repo(git_repository *repo)
{
	int i;
	int err = GIT_OK;
	int rc = EXIT_FAILURE;
	int autocommit = 1;

	git_merge_analysis_t analysis;
	git_merge_preference_t preference;

	const char *commit_str = "origin/master";
	git_reference *commit_ref = NULL;
	git_object *commit_obj = NULL;
	git_annotated_commit *commit_merge_head = NULL;

	git_reference *head_ref = NULL;

	git_tree *commit_tree_obj = NULL;
	git_reference *new_ref = NULL;

	git_merge_options merge_options = GIT_MERGE_OPTIONS_INIT;
	git_checkout_options checkout_options = GIT_CHECKOUT_OPTIONS_INIT;

	git_index *index = NULL;

	if ((err = git_branch_lookup(&commit_ref, repo, commit_str, GIT_BRANCH_LOCAL)))
	{
		if (err == GIT_ENOTFOUND)
		{
			if ((err = git_branch_lookup(&commit_ref, repo, commit_str, GIT_BRANCH_REMOTE)))
				goto out;
		} else
		{
			goto out;
		}
	}
	if ((err = git_annotated_commit_from_ref(&commit_merge_head, repo, commit_ref)))
		goto out;

	if ((err = git_merge_analysis(&analysis, &preference, repo, (const git_annotated_commit **)&commit_merge_head, 1)))
		goto out;

	checkout_options.checkout_strategy = 1;
    

	if (analysis & GIT_MERGE_ANALYSIS_FASTFORWARD && preference != GIT_MERGE_PREFERENCE_NO_FASTFORWARD)
	{
		fprintf(stderr, "Fast forward merge\n");

		if ((err = git_reference_peel(&commit_obj, commit_ref, GIT_OBJ_COMMIT)))
			goto out;

		if ((err = git_repository_head(&head_ref,repo)))
			goto out;

		if ((err = git_commit_tree(&commit_tree_obj,(git_commit*)commit_obj)))
			goto out;

		if ((err = git_checkout_tree(repo, (git_object*)commit_tree_obj, &checkout_options)))
			goto out;

		if ((err = git_reference_set_target(&new_ref, head_ref, git_commit_id((git_commit*)commit_obj), NULL, NULL)))
			goto out;

		goto out;
	}

	if ((err = git_merge(repo, (const git_annotated_commit **)&commit_merge_head, 1, &merge_options, &checkout_options)))
		goto out;

	if ((err = git_repository_index(&index, repo)))
		goto out;

	if (!git_index_has_conflicts(index))
	{
		if (autocommit)
		{
			char *argv[3];
			char message[256];
			sprintf(message, "Merged branch '%s'",commit_str);
			argv[0] = "commit";
			argv[1] = "-m";
			argv[2] = message;
			cmd_commit_repo(repo, 3, argv);
		}
	} else
	{
		printf("conflict during merge! Please resolve and commit\n");
	}
out:
    check_error();

	if (index) git_index_free(index);
	if (commit_merge_head) git_annotated_commit_free(commit_merge_head);
	if (commit_obj) git_object_free(commit_obj);
	if (commit_ref) git_reference_free(commit_ref);
	if (head_ref) git_reference_free(head_ref);
	if (commit_tree_obj) git_tree_free(commit_tree_obj);
	if (new_ref) git_reference_free(new_ref);

	return rc;
}

/*implements the function of "git merge origin/master"*/
void cmd_merge()
{
    git_repository *repo = NULL;
    git_libgit2_init();
    git_repository_open(&repo, LOCALGIT);

    {
        cmd_merge_repo(repo);
    }
    git_repository_free(repo);
    git_libgit2_shutdown();
}


int cmd_rebase_repo(git_repository *repo)
{
	int err = GIT_OK;
	int rc = EXIT_FAILURE;

	git_signature *sig = NULL;

	const char *upstream_str;
	git_reference *upstream_ref = NULL;
	git_annotated_commit *upstream = NULL;

	git_reference *branch_ref = NULL;
	git_annotated_commit *branch = NULL;

	git_rebase *rebase = NULL;

	int abort = 0;
	int cont = 0;

	//upstream_str = argv[1];
    upstream_str = "FETCH_HEAD";
	if (!strcmp(upstream_str, "--abort")) abort = 1;
	else if (!strcmp(upstream_str, "--continue")) cont = 1;

	if ((err = sgit_get_author_signature(repo, &sig)))
		goto out;

	if (abort) {
		if ((err = git_rebase_open(&rebase, repo, NULL)))
			goto out;

		if ((err = git_rebase_abort(rebase)))
			goto out;
	} else {
		git_rebase_operation *oper;
		git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
		git_rebase_options rebase_opts = GIT_REBASE_OPTIONS_INIT;

		checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;

		if (cont)
		{
			if ((err = git_rebase_open(&rebase, repo, NULL)))
				goto out;
		} else
		{
            if ((err = git_reference_dwim(&upstream_ref, repo, upstream_str))) {
                int t = 0;
                t = 4;
				goto out;
            }
			if ((err = git_annotated_commit_from_ref(&upstream, repo, upstream_ref)))
				goto out;

			if ((err = git_repository_head(&branch_ref,repo)) < 0)
				goto out;
			if ((err = git_annotated_commit_from_ref(&branch, repo, branch_ref)))
				goto out;

			if ((err = git_rebase_init(&rebase, repo, branch, upstream, NULL, NULL)))
				goto out;
		}

		while (!(err = git_rebase_next(&oper, rebase)))
		{
			git_oid oid;
			if ((err = git_rebase_commit(&oid, rebase, NULL, sig, NULL, NULL)))
				goto out;
		}

		if (err != GIT_ITEROVER && err != GIT_OK)
			goto out;

		if ((err = git_rebase_finish(rebase, sig)))
			goto out;
	}
out:
    check_error();

	if (rebase) git_rebase_free(rebase);
	if (upstream) git_annotated_commit_free(upstream);
	if (upstream_ref) git_reference_free(upstream_ref);

	if (branch) git_annotated_commit_free(branch);
	if (branch_ref) git_reference_free(branch_ref);
	if (sig) git_signature_free(sig);
	return rc;
}

void cmd_rebase()
{
    git_repository *repo = NULL;

    git_libgit2_init();
    git_repository_open(&repo, LOCALGIT);

    cmd_rebase_repo(repo);

    git_repository_free(repo);
    git_libgit2_shutdown();
}

int cmd_commit_repo(git_repository *repo)
{
	int err = 0;
	git_reference *head = NULL;
	git_reference *branch = NULL;
	git_commit *parent = NULL;
	git_oid tree_oid;
	git_oid commit_oid;
	git_commit *commit = NULL;

	git_index *idx = NULL;

	git_signature *author_signature = NULL;
	git_signature *committer_signature = NULL;
	git_tree *tree = NULL;
	git_commit **parents = NULL;
    //TODO: write the message for commit
    const char *message = "demoCommit";

	int i;
	int rc = EXIT_FAILURE;
	int num_parents = 0;

	/* Count the number of parents */
	if ((git_repository_head(&head,repo)) == GIT_OK)
		num_parents++;

	err = sgit_repository_mergeheads_count(&num_parents, repo);
	if (err && err != GIT_ENOTFOUND)
		goto out;

	/* Now determine the actual parents */
	if (num_parents)
	{
		if (!(parents = (git_commit **)malloc(sizeof(*parents)*num_parents)))
		{
			fprintf(stderr,"Not enough memory!\n");
			goto out;
		}

		if ((err = git_reference_peel((git_object**)&parent,head,GIT_OBJ_COMMIT)))
			goto out;
		parents[0] = parent;

		if (num_parents > 1)
		{
			struct mergehead_peel_payload peel_payload;

			peel_payload.repo = repo;
			peel_payload.next_parent = parents + 1;

			if ((err = git_repository_mergehead_foreach(repo, mergehead_peel_cb, &peel_payload)))
				goto out;
		}
	}

	if ((err = sgit_get_author_signature(repo, &author_signature)) != GIT_OK)
		goto out;

	if ((err = sgit_get_committer_signature(repo, &committer_signature)) != GIT_OK)
		goto out;

	/* Write index as tree */
	if ((err = git_repository_index(&idx,repo)) != GIT_OK)
		goto out;
	if (git_index_entrycount(idx) == 0)
	{
		fprintf(stderr,"Nothing to commit!\n");
		goto out;
	}
	if ((err = git_index_write_tree_to(&tree_oid, idx, repo)) != GIT_OK)
		goto out;
	if ((err = git_tree_lookup(&tree,repo,&tree_oid)) != GIT_OK)
		goto out;

	/* Write tree as commit */
	if ((err = git_commit_create(&commit_oid, repo, "HEAD", author_signature, committer_signature,
				NULL, message, tree, num_parents, (const git_commit**)parents)) != GIT_OK)
		goto out;

	rc = EXIT_SUCCESS;
out:
    check_error();
	if (head) git_reference_free(head);
	if (tree) git_tree_free(tree);
	if (idx) git_index_free(idx);
	if (parents)
	{
		for (i=0;i<num_parents;i++)
			git_commit_free(parents[i]);
		free(parents);
	}
	if (author_signature) git_signature_free(author_signature);
	if (committer_signature) git_signature_free(committer_signature);
	if (commit) git_commit_free(commit);
	if (branch) git_reference_free(branch);
	return rc;
}

/*implements the function of "git commit -a -m demoCommit"*/
void cmd_commit()
{
    git_repository *repo = NULL;

    git_libgit2_init();
    git_repository_open(&repo, LOCALGIT);

    cmd_commit_repo(repo);

    git_repository_free(repo);
    git_libgit2_shutdown();
}

typedef void (*git_fun)();

struct cmd_struct {
    char *cmd;
    git_fun fun;
};

int main(int argc, char *argv[])
{
    int i;
    struct cmd_struct cmds[] = {
        {"init", cmd_init},
        {"clone", cmd_clone},
        {"add", cmd_add},
        {"commit", cmd_commit},
        {"push", cmd_push},
        {"fetch", cmd_fetch},
        {"merge", cmd_merge},
        {NULL, NULL}
    };

    if (argc != 2) {
        printf("Please use the command as ./base clone, ./base add for running");
        return 0;
    }
    

    for (i = 0; cmds[i].cmd != NULL; ++i) {
        if (!strcmp(argv[1], cmds[i].cmd)) {
            cmds[i].fun();
            break;
        }
    }
	return 0;
}


