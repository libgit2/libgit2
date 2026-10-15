/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include <git2.h>
#include "common.h"
#include "cmd.h"
#include "futils.h"
#include "process.h"

#define COMMAND_NAME "commit"

static char unset_keyid[] = { 0 };

static char *message;
static char *template;
static int amend;
static int allow_empty;
static int allow_empty_message;
static char *keyid = unset_keyid;
static int no_sign;

static const cli_opt_spec opts[] = {
	CLI_COMMON_OPT,

	{ CLI_OPT_TYPE_VALUE,     NULL,                 'm', &message,             0,
	  CLI_OPT_USAGE_DEFAULT, "message",             "specify the commit message" },
	{ CLI_OPT_TYPE_VALUE,    "template",            't', &template,            0,
	  CLI_OPT_USAGE_DEFAULT, "file",                "the original text for the commit message" },

	{ CLI_OPT_TYPE_SWITCH,   "amend",                0,  &amend,               1,
	  0,                      NULL,                 "allow commits that have no changes" },

	{ CLI_OPT_TYPE_SWITCH,   "allow-empty",          0,  &allow_empty,         1,
	  0,                      NULL,                 "allow commits that have no changes" },
	{ CLI_OPT_TYPE_SWITCH,   "allow-empty-message",  0,  &allow_empty_message, 1,
	  0,                      NULL,                 "allow commit message" },

	{ CLI_OPT_TYPE_VALUE,    "gpg-sign",            'S', &keyid,               0,
	  CLI_OPT_USAGE_VALUE_OPTIONAL, "keyid",        "sign the commit" },
	{ CLI_OPT_TYPE_SWITCH,   "no-gpg-sign",          0,  &no_sign,             1,
	  CLI_OPT_USAGE_CHOICE,   NULL,                 "do not sign the commit" },

	{ 0 },
};

#define MESSAGE_INSTRUCTIONS \
	"# Please enter the commit message for your changes. Lines starting\n" \
	"# with '#' will be ignored, and an empty message aborts the commit.\n"

static void print_help(void)
{
	cli_opt_usage_fprint(stdout, PROGRAM_NAME, COMMAND_NAME, opts, 0);
	printf("\n");

	printf("Commit the current contents of the index.\n");
	printf("\n");

	printf("Options:\n");

	cli_opt_help_fprint(stdout, opts);
}

static int check_empty_commit(git_status_list *status)
{
	const git_status_entry *entry;
	size_t entries, i;

	if (amend || allow_empty)
		return 0;

	entries = git_status_list_entrycount(status);

	for (i = 0; i < entries; i++) {
		entry = git_status_byindex(status, i);

		if (entry->head_to_index != NULL)
			return 0;
	}

	return cli_error("no changes added to commit (use \"git add\" and/or \"git commit -a\")");
}

static int prepare_amend_message(
	git_str *message,
	git_reference *head_ref)
{
	git_commit *head_commit;
	int ret = 0;

	if (git_reference_peel((git_object **)&head_commit, head_ref, GIT_OBJECT_COMMIT) < 0) {
		ret = cli_error_git();
		goto done;
	}

	git_str_puts(message, git_commit_message(head_commit));

done:
	git_commit_free(head_commit);
	return ret;
}

static int prepare_template(git_str *message, git_repository *repo)
{
	git_config *config = NULL;
	git_buf config_value = GIT_BUF_INIT;
	int error, ret = 0;

	if (!template) {
		if (git_repository_config(&config, repo) < 0) {
			ret = cli_error_git();
			goto done;
		}

		error = git_config_get_string_buf(&config_value,
			config, "commit.template");

		if (error && error != GIT_ENOTFOUND) {
			ret = cli_error_git();
			goto done;
		} else if (error != GIT_ENOTFOUND) {
			template = config_value.ptr;
		}
	}

	if (template) {
		if (git_futils_readbuffer(message, template) < 0) {
			ret = cli_error_git();
			goto done;
		}
	}

done:
	git_buf_dispose(&config_value);
	git_config_free(config);
	return ret;
}

static int prepare_changes(
	git_str *message,
	git_status_list *status)
{
	const git_status_entry *entry;
	const git_diff_delta *delta;
	const char *change_type, *file;
	bool staged_header = false, unstaged_header = false,
	     untracked_header = false;
	size_t entries, i;

	entries = git_status_list_entrycount(status);

	for (i = 0; i < entries; i++) {
		entry = git_status_byindex(status, i);

		if (!(delta = entry->head_to_index))
			continue;

		if (!staged_header) {
			git_str_puts(message, "# Changes to be committed:\n");
			staged_header = true;
		}

		switch (delta->status) {
		case GIT_DELTA_ADDED:
			change_type = "new file:";
			file = delta->new_file.path;
			break;
		case GIT_DELTA_DELETED:
			change_type = "deleted:";
			file = delta->old_file.path;
			break;
		case GIT_DELTA_MODIFIED:
			change_type = "modified:";
			file = delta->old_file.path;
			break;
		case GIT_DELTA_RENAMED:
			change_type = "renamed:";
			file = delta->old_file.path;
			break;
		default:
			CLI_ASSERT(!"unhandled head->index delta type");
			break;
		}


		git_str_printf(message, "#\t%-11s %s", change_type, file);

		if (delta->status == GIT_DELTA_RENAMED)
			git_str_printf(message, " -> %s", delta->new_file.path);

		git_str_printf(message, "\n");
	}

	for (i = 0; i < entries; i++) {
		entry = git_status_byindex(status, i);

		if (!(delta = entry->index_to_workdir) ||
		    delta->status == GIT_DELTA_UNTRACKED)
			continue;

		if (!unstaged_header) {
			if (staged_header)
				git_str_puts(message, "#\n");

			git_str_puts(message, "# Changes not staged for commit:\n");
			unstaged_header = true;
		}

		switch (delta->status) {
		case GIT_DELTA_DELETED:
			change_type = "deleted:";
			file = delta->old_file.path;
			break;
		case GIT_DELTA_MODIFIED:
			change_type = "modified:";
			file = delta->old_file.path;
			break;
		default:
			printf("%d\n", delta->status);
			CLI_ASSERT(!"unhandled workdir->index delta type");
			break;
		}

		git_str_printf(message, "#       %-11s %s", change_type, file);

		if (delta->status == GIT_DELTA_RENAMED)
			git_str_printf(message, " -> %s", delta->new_file.path);

		git_str_printf(message, "\n");
	}

	for (i = 0; i < entries; i++) {
		entry = git_status_byindex(status, i);

		if (!(delta = entry->index_to_workdir) ||
		    delta->status != GIT_DELTA_UNTRACKED)
			continue;

		if (!untracked_header) {
			if (staged_header || unstaged_header)
				git_str_puts(message, "#\n");

			git_str_puts(message, "# Untracked files:\n");
			untracked_header = true;
		}

		git_str_printf(message, "#       %s\n", delta->new_file.path);
	}

	if (git_str_oom(message))
		return cli_error_git();

	return 0;
}

static int prepare_initial_message(
	git_str *message,
	git_repository *repo,
	git_status_list *status)
{
	git_reference *head_ref = NULL;
	const char *branch_name = NULL;
	int ret;

	if (git_repository_head(&head_ref, repo) < 0) {
		ret = cli_error_git();
		goto done;
	}

	if (git_branch_name(&branch_name, head_ref) < 0)
		git_error_clear();

	if (amend)
		ret = prepare_amend_message(message, head_ref);
	else
		ret = prepare_template(message, repo);

	if (ret < 0)
		goto done;

	git_str_puts(message, "\n" MESSAGE_INSTRUCTIONS "#\n");

	if (branch_name)
		git_str_printf(message, "# On branch %s\n", branch_name);
	else
		git_str_printf(message, "# Not currently on any branch.\n");

	ret = prepare_changes(message, status);

done:
	git_reference_free(head_ref);
	return ret;
}

static int should_sign(bool *out, git_repository *repo)
{
	git_config *config = NULL;
	int sign, error, ret = 0;

	if (no_sign) {
		*out = false;
		return 0;
	}

	if (keyid != unset_keyid) {
		*out = true;
		return 0;
	}

	if (git_repository_config(&config, repo) < 0) {
		ret = cli_error_git();
		goto done;
	}

	error = git_config_get_bool(&sign, config, "commit.gpgsign");

	if (error == GIT_ENOTFOUND)
		*out = false;
	else
		*out = !!sign;

done:
	git_config_free(config);
	return ret;
}

static int lookup_editor(char **out, git_repository *repo)
{
	git_str env = GIT_STR_INIT;
	git_str term = GIT_STR_INIT;
	git_buf buf = GIT_BUF_INIT;
	git_config *config = NULL;
	bool is_dumb = true;
	int ret = 0;

	if (git__getenv(&env, "GIT_EDITOR") == 0) {
		*out = git_str_detach(&env);
		goto done;
	}

	if (git_repository_config(&config, repo) < 0) {
		ret = cli_error_git();
		goto done;
	}

	if (git_config_get_string_buf(&buf, config, "core.editor") == 0) {
		*out = git__strdup(buf.ptr);

		if (*out == NULL)
			ret = -1;

		goto done;
	}

	if (git__getenv(&term, "TERM") == 0)
		is_dumb = (git__strcmp(term.ptr, "dumb") == 0);

	if ((!is_dumb && git__getenv(&env, "VISUAL") == 0) ||
	    git__getenv(&env, "EDITOR") == 0) {
		*out = git_str_detach(&env);
		goto done;
	}

	if (is_dumb) {
		ret = cli_error("no EDITOR is specified");
	} else {
		*out = git__strdup("vi");
		ret = *out == NULL ? -1 : 0;
	}

done:
	git_buf_dispose(&buf);
	git_str_dispose(&term);
	git_str_dispose(&env);
	git_config_free(config);
	return ret;
}

static int read_commit_message(
	git_buf *out,
	git_repository *repo,
	git_status_list *status)
{
	git_process *process = NULL;
	git_process_result result = GIT_PROCESS_RESULT_INIT;
	char *editor = NULL;
	git_str cmdline = GIT_STR_INIT;
	git_str message_file = GIT_STR_INIT;
	git_str initial = GIT_STR_INIT;
	git_str message = GIT_STR_INIT;
	int ret = 0;

	if ((ret = prepare_initial_message(&initial, repo, status)) != 0)
		return ret;

	if (git_str_joinpath(&message_file,
			git_repository_path(repo),
			"COMMIT_EDITMSG") < 0 ||
	    git_futils_writebuffer(&initial, message_file.ptr,
			O_CREAT|O_TRUNC|O_WRONLY, 0666) < 0) {
		ret = cli_error_git();
		goto done;
	}

	if ((ret = lookup_editor(&editor, repo)) != 0)
		goto done;

	if (git_str_printf(&cmdline, "%s \"%s\"",
			editor, message_file.ptr) < 0 ||
	    git_process_new_from_cmdline(&process, cmdline.ptr,
			NULL, 0, NULL) < 0 ||
	    git_process_start(process) < 0 ||
	    git_process_wait(&result, process) < 0) {
		ret = cli_error_git();
		goto done;
	}

	if (result.status != GIT_PROCESS_STATUS_NORMAL ||
	    result.exitcode != 0) {
		ret = cli_error("could not invoke '%s': exit code %d",
			editor, result.exitcode);
		ret = 1;
		goto done;
	}

	if (git_futils_readbuffer(&message, message_file.ptr) < 0 ||
	    git_message_prettify(out, message.ptr, 1, '#') < 0) {
		ret = cli_error_git();
		goto done;
	}

	ret = 0;

done:
	git_process_free(process);
	git_str_dispose(&message);
	git_str_dispose(&cmdline);
	git_str_dispose(&message_file);
	git_str_dispose(&initial);
	git__free(editor);
	return ret;
}

static int sign_commit(
	git_commitbuilder *builder,
	git_repository *repo,
	const char *commit_content,
	void *payload)
{
	git_process *process = NULL;
	git_config *config = NULL;
	git_buf config_value = GIT_BUF_INIT;
	git_str committer_ident = GIT_STR_INIT, cmdline = GIT_STR_INIT,
		signature = GIT_STR_INIT;
	const char *args[3] = { "gpg", "-bsau", NULL };
	git_process_result result = GIT_PROCESS_RESULT_INIT;
	git_process_options process_opts = GIT_PROCESS_OPTIONS_INIT;
	git_signature *committer = NULL;
	char buf[16];
	ssize_t read_len;
	int ret = 0;

	GIT_UNUSED(builder);
	GIT_UNUSED(commit_content);
	GIT_UNUSED(payload);

	if (git_repository_config(&config, repo) < 0) {
		ret = cli_error_git();
		goto done;
	}

	if (git_config_get_string_buf(&config_value, config, "gpg.program") == 0)
		args[0] = config_value.ptr;

	if (keyid && keyid != unset_keyid) {
		args[2] = keyid;
	} else {
		if (git_signature_default_from_env(NULL, &committer, repo) < 0 ||
		    git_str_printf(&committer_ident, "%s <%s>", committer->name, committer->email) < 0) {
			ret = cli_error_git();
			goto done;
		}

		args[2] = committer_ident.ptr;
	}

	process_opts.capture_in = 1;
	process_opts.capture_out = 1;

	if (git_process_new(&process, args, 3, NULL, 0, &process_opts) < 0 ||
	    git_process_start(process) < 0 ||
	    git_process_write(process, commit_content, strlen(commit_content)) < 0) {
		ret = cli_error_git();
		goto done;
	}

	git_process_close_in(process);

	while ((read_len = git_process_read(process, buf, sizeof(buf))) > 0) {
		if (git_str_put(&signature, buf, (size_t)read_len) < 0) {
			ret = cli_error_git();
			goto done;
		}
	}

	if (read_len < 0) {
		ret = cli_error_git();
		goto done;
	}

	if (git_process_wait(&result, process) < 0 ||
	    git_commitbuilder_add_header(builder, "gpgsig", signature.ptr) < 0) {
		ret = cli_error_git();
		goto done;
	}

	ret = result.exitcode == 0 ? 0 : GIT_EUSER;

done:
	git_signature_free(committer);
	git_buf_dispose(&config_value);
	git_str_dispose(&cmdline);
	git_str_dispose(&committer_ident);
	git_str_dispose(&signature);
	git_process_close(process);
	git_process_free(process);

	return ret;
}

int cmd_commit(int argc, char **argv)
{
	cli_repository_open_options open_opts = { argv + 1, argc - 1};
	git_status_options status_opts = GIT_STATUS_OPTIONS_INIT;
	git_commit_create_options commit_opts = GIT_COMMIT_CREATE_OPTIONS_INIT;
	git_repository *repo = NULL;
	git_index *index = NULL;
	git_status_list *status = NULL;
	git_tree *tree = NULL;
	git_oid commit_id, tree_id;
	git_buf read_message = GIT_BUF_INIT;
	cli_opt invalid_opt;
	bool sign = false;
	int error, ret = 0;

	if (cli_opt_parse(&invalid_opt, opts, argv + 1, argc - 1, CLI_OPT_PARSE_GNU))
		return cli_opt_usage_error(COMMAND_NAME, opts, &invalid_opt);

	status_opts.flags |= GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX;
	status_opts.flags |= GIT_STATUS_OPT_INCLUDE_UNTRACKED;

	/* We check for empty commits; don't duplicate the check. */
	commit_opts.allow_empty_commit = true;
	commit_opts.payload = repo;

	if (cli_opt__show_help) {
		print_help();
		return 0;
	}

	if (cli_repository_open(&repo, &open_opts) < 0 ||
	    git_repository_index(&index, repo) < 0) {
		ret = cli_error_git();
		goto done;
	}

	if ((ret = should_sign(&sign, repo)) != 0)
		goto done;

	if (sign)
		commit_opts.sign = sign_commit;

	if (git_status_list_new(&status, repo, &status_opts) < 0) {
		ret = cli_error_git();
		goto done;
	}

	if ((ret = check_empty_commit(status)) != 0)
		goto done;

	if (git_index_write_tree(&tree_id, index) < 0 ||
	    git_tree_lookup(&tree, repo, &tree_id) < 0) {
		ret = cli_error_git();
		goto done;
	}

	if (!message) {
		if ((ret = read_commit_message(&read_message,
				repo, status)) != 0)
			goto done;

		message = read_message.ptr;
	}

	if (!*message && !allow_empty_message) {
		ret = cli_error("no commit message specified (use \"--allow-empty-message\")");
		goto done;
	}

	if (amend)
		error = git_commit_amend_from_tree(&commit_id,
			repo, tree, message, &commit_opts);
	else
		error = git_commit_create_from_tree(&commit_id,
			repo, tree, message, &commit_opts);

	if (error == GIT_EUSER)
		ret = CLI_EXIT_GIT;
	else if (error < 0)
		ret = cli_error_git();

done:
	git_buf_dispose(&read_message);
	git_status_list_free(status);
	git_index_free(index);
	git_repository_free(repo);
	return ret;
}
