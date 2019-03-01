/**
 * API for emscripten to run in node / browser
 * 
 * Author: Peter Johan Salomonsen ( https://github.com/petersalomonsen ) 
 */

#include <emscripten.h>

#include <stdio.h>
#include "streams/stransport.h"
#include "streams/tls.h"

#ifdef EMSCRIPTEN_NODEJS
#include "streams/emscripten_nodejs.h"
#else
#include "streams/emscripten_browser.h"
#endif

#include "git2.h"
#include "git2/clone.h"
#include "git2/merge.h"
#include "filter.h"
#include "git2/sys/filter.h"

static git_repository *repo = NULL;
int merge_file_favor = GIT_MERGE_FILE_FAVOR_NORMAL;

typedef struct progress_data {
	git_transfer_progress fetch_progress;
	size_t completed_steps;
	size_t total_steps;
	const char *path;
} progress_data;

static void print_progress(const progress_data *pd)
{
	int network_percent = pd->fetch_progress.total_objects > 0 ?
		(100*pd->fetch_progress.received_objects) / pd->fetch_progress.total_objects :
		0;
	int index_percent = pd->fetch_progress.total_objects > 0 ?
		(100*pd->fetch_progress.indexed_objects) / pd->fetch_progress.total_objects :
		0;

	int checkout_percent = pd->total_steps > 0
		? (100 * pd->completed_steps) / pd->total_steps
		: 0;
	int kbytes = pd->fetch_progress.received_bytes / 1024;

	char *progress_string;
	if (pd->fetch_progress.total_objects &&
		pd->fetch_progress.received_objects == pd->fetch_progress.total_objects) {
		asprintf(&progress_string,"Resolving deltas %d/%d\n",
		       pd->fetch_progress.indexed_deltas,
		       pd->fetch_progress.total_deltas);
	} else {
		asprintf(&progress_string,"net %3d%% (%4d kb, %5d/%5d)  /  idx %3d%% (%5d/%5d)  /  chk %3d%% (%4" PRIuZ "/%4" PRIuZ ") %s\n",
		   network_percent, kbytes,
		   pd->fetch_progress.received_objects, pd->fetch_progress.total_objects,
		   index_percent, pd->fetch_progress.indexed_objects, pd->fetch_progress.total_objects,
		   checkout_percent,
		   pd->completed_steps, pd->total_steps,
		   pd->path);
	}
	EM_ASM_({jsgitprogresscallback(UTF8ToString($0));}, progress_string);
	free(progress_string);
}

static int sideband_progress(const char *str, int len, void *payload)
{
	(void)payload; // unused

	char *progress_string;
	asprintf(&progress_string, "remote: %.*s\n", len, str);
	EM_ASM_({jsgitprogresscallback(UTF8ToString($0));}, progress_string);
	free(progress_string);
	return 0;
}

static int fetch_progress(const git_transfer_progress *stats, void *payload)
{
	progress_data *pd = (progress_data*)payload;
	pd->fetch_progress = *stats;
	print_progress(pd);
	return 0;
}
static void checkout_progress(const char *path, size_t cur, size_t tot, void *payload)
{
	progress_data *pd = (progress_data*)payload;
	pd->completed_steps = cur;
	pd->total_steps = tot;
	pd->path = path;
	print_progress(pd);
}

int cred_acquire_cb(git_cred **out,
	const char * url,
	const char * username_from_url,
	unsigned int allowed_types,
	void * payload)
{	
	int error;

	error = git_cred_userpass_plaintext_new(out, "username", "password");

	return error;
}

/**
 * Apply headers to fetch options from Module.jsgitheaders
 */
int fetch_applyheaders(git_strarray * headers) {
	int has_headers = EM_ASM_INT({
		return Module.jsgitheaders && Module.jsgitheaders.length > 0 ? 1 : 0;
	});
	
	if (has_headers) {	
		headers->count = EM_ASM_({return Module.jsgitheaders.length;});
		headers->strings = malloc(sizeof(char*) * headers->count);
		EM_ASM_({		
			Module.jsgitheaders.forEach((headerObj, ndx) => {
				const header = `${headerObj.name}: ${headerObj.value}`;
				const byteLen = lengthBytesUTF8(header) + 1;
				const strPtr = _malloc(byteLen);
				stringToUTF8(header, strPtr, byteLen);
				setValue($0 + $1 * ndx, strPtr, '*');		
			});
		}, headers->strings, sizeof(char*));
	}

	return has_headers;
}

/**
 * Free allocated strings for headers
 */
void fetch_freeheaders(git_strarray * headers) {
	for (int n = 0;n < headers->count; n++) {
		free(headers->strings[n]);
	}
	free(headers->strings);
}

int cloneremote(const char * url,const char * path)
{
	progress_data pd = {{0}};

	git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
	git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
		
	int error;
	
	// Set up options
	checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
	checkout_opts.progress_cb = checkout_progress;
	checkout_opts.progress_payload = &pd;
	clone_opts.checkout_opts = checkout_opts;
	clone_opts.fetch_opts.callbacks.sideband_progress = sideband_progress;
	clone_opts.fetch_opts.callbacks.transfer_progress = &fetch_progress;
	clone_opts.fetch_opts.callbacks.credentials = cred_acquire_cb;
	clone_opts.fetch_opts.callbacks.payload = &pd;
		
	git_strarray headers;
	int has_headers = fetch_applyheaders(&headers);
	if(has_headers) {
		clone_opts.fetch_opts.custom_headers = headers;
	}

	// Do the clone
	error = git_clone(&repo, url, path, &clone_opts);
	
	// Free allocated strings for headers
	if(has_headers) {
		fetch_freeheaders(&headers);
	}
	
	printf("\n");
	if (error != 0) {
		const git_error *err = giterr_last();
		if (err) printf("ERROR %d: %s\n", err->klass, err->message);
		else printf("ERROR %d: no detailed info\n", error);
	}	
	return error;
}

/**
 * This function gets called for each remote-tracking branch that gets
 * updated. The message we output depends on whether it's a new one or
 * an update.
 */
static int update_cb(const char *refname, const git_oid *a, const git_oid *b, void *data)
{
	printf("Update cb %s\n",refname);
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

/**
 * This gets called during the download and indexing. Here we show
 * processed and total objects in the pack and the amount of received
 * data. Most frontends will probably want to show a percentage and
 * the download rate.
 */
static int transfer_progress_cb(const git_transfer_progress *stats, void *payload)
{
	(void)payload;

	if (stats->received_objects == stats->total_objects) {
		printf("Resolving deltas %d/%d\n",
			stats->indexed_deltas, stats->total_deltas);
	} else if (stats->total_objects > 0) {
		printf("Received %d/%d objects (%d) in %" PRIuZ " bytes\n",
			stats->received_objects, stats->total_objects,
			stats->indexed_objects, stats->received_bytes);
	}
	return 0;
}

void printLastError() {
	const git_error *err = giterr_last();
	if (err) printf("ERROR %d: %s\n", err->klass, err->message);	
}

void EMSCRIPTEN_KEEPALIVE jsgitinit() {
#ifdef EMSCRIPTEN_NODEJS
	git_stream_register_tls(git_open_emscripten_nodejs_stream);
#else
	git_stream_register_tls(git_open_emscripten_stream);
#endif
	git_libgit2_init();	
	printf("libgit2 for javascript initialized\n");
}

/**
 * Initialize repository in current directory
 */
void EMSCRIPTEN_KEEPALIVE jsgitinitrepo(unsigned int bare) {
	git_repository_init(&repo, ".", bare);
}

/**
 * Open repository in current directory
 */
void EMSCRIPTEN_KEEPALIVE jsgitopenrepo() {
	git_repository_open(&repo, ".");
}

void EMSCRIPTEN_KEEPALIVE jsgitclone(char * url, char * localdir) {			
	cloneremote(url,localdir);		
}

void EMSCRIPTEN_KEEPALIVE jsgitadd(const char * path) {	
	git_index *index;	
	git_repository_index(&index, repo);	
	git_index_add_bypath(index, path);
	git_index_write(index);
	git_index_free(index);
}

void EMSCRIPTEN_KEEPALIVE jsgitremove(const char * path) {	
	git_index *index;	
	git_repository_index(&index, repo);	
	git_index_remove_bypath(index, path);
	git_index_write(index);
	git_index_free(index);
}

void EMSCRIPTEN_KEEPALIVE jsgitcommit(char * comment) {
	git_oid commit_oid,tree_oid,oid_parent_commit;
	git_commit *parent_commit;
	git_tree *tree;
	git_index *index;	
	git_object *parent = NULL;
	git_reference *ref = NULL;	
	
	git_revparse_ext(&parent, &ref, repo, "HEAD");
	git_repository_index(&index, repo);	
	git_index_write_tree(&tree_oid, index);
	git_index_write(index);
	git_index_free(index);

	int error = git_tree_lookup(&tree, repo, &tree_oid);
	if (error != 0) {
		const git_error *err = giterr_last();
		if (err) printf("ERROR %d: %s\n", err->klass, err->message);
		else printf("ERROR %d: no detailed info\n", error);
	}

	
	git_signature *signature;	
	git_signature_default(&signature, repo);
	
	error = git_commit_create_v(
		&commit_oid,
		repo,
		"HEAD",
		signature,
		signature,
		NULL,
		comment,
		tree,
		parent ? 1 : 0, parent);
		
	if (error != 0) {
		const git_error *err = giterr_last();
		if (err) printf("ERROR %d: %s\n", err->klass, err->message);
		else printf("ERROR %d: no detailed info\n", error);
	}
	git_signature_free(signature);
	git_tree_free(tree);	
}

int jsgitrepositorystate() {
	return git_repository_state(repo);
}

void EMSCRIPTEN_KEEPALIVE jsgitprintlatestcommit()
{
	int rc;
	git_commit * commit = NULL; /* the result */
	git_oid oid_parent_commit;  /* the SHA1 for last commit */

	/* resolve HEAD into a SHA1 */
	rc = git_reference_name_to_id( &oid_parent_commit, repo, "HEAD" );
	if ( rc == 0 )
	{
		/* get the actual commit structure */
		rc = git_commit_lookup( &commit, repo, &oid_parent_commit );
		if ( rc == 0 )
		{
			printf("%s\n",git_commit_message(commit));
			git_commit_free(commit);      
		}
	}	
}


void jsgithistoryvisitcommit(git_commit *c)
{
	size_t i, num_parents = git_commit_parentcount(c);

	char oidstr[GIT_OID_HEXSZ + 1];
	
	git_oid_tostr(oidstr, sizeof(oidstr), git_commit_id(c));

	const git_signature * author = git_commit_author(c);
	const char * message = git_commit_message(c);
	
	int alreadyvisited = EM_ASM_INT({
				var commitentry = ({
					id: UTF8ToString($0),
					when: $4,
					name: UTF8ToString($1),
					email: UTF8ToString($2),
					message: UTF8ToString($3),
					parents: []
				});

				if(jsgithistoryresultbyid[commitentry.id]) {
					return 1;
				} else {
					jsgithistoryresultbyid[commitentry.id] = commitentry;
					jsgithistoryresult.push(commitentry);
					return 0;
				}
			},
			oidstr,			
			author->name,
			author->email,
			message,
			(uint32_t)author->when.time
	);
	
	if(alreadyvisited == 0) {
		for (i=0; i<num_parents; i++) {		
			git_commit *p;
			if (!git_commit_parent(&p, c, i)) {
				
				char parent_oidstr[GIT_OID_HEXSZ + 1];
				git_oid_tostr(parent_oidstr, sizeof(parent_oidstr), git_commit_id(p));
				
				EM_ASM_({
					var commitid = UTF8ToString($0);
					jsgithistoryresultbyid[commitid].parents.push(UTF8ToString($1));
				}, oidstr, parent_oidstr);
				
				jsgithistoryvisitcommit(p);
			}
			git_commit_free(p);
		}
	}
}

void EMSCRIPTEN_KEEPALIVE jsgithistory() {
  	git_commit *commit;
	git_oid oid_parent_commit;
	
	EM_ASM(
		jsgithistoryresult = [];
		jsgithistoryresultbyid = {};
	);

	int rc = git_reference_name_to_id( &oid_parent_commit, repo, "HEAD" );

	if ( rc == 0 )
	{
		rc = git_commit_lookup( &commit, repo, &oid_parent_commit );
		if ( rc == 0 )
		{
  			jsgithistoryvisitcommit(commit);			
			git_commit_free(commit);      
		}
	}	  	
}

void EMSCRIPTEN_KEEPALIVE jsgitshutdown() {
	git_repository_free(repo);
	git_libgit2_shutdown();
}

int fetchead_foreach_cb(const char *ref_name,
	const char *remote_url,
	const git_oid *oid,
	unsigned int is_merge,
	void *payload)
{	  
	if(is_merge) {
		git_annotated_commit * fetchhead_annotated_commit;					

		git_annotated_commit_lookup(&fetchhead_annotated_commit,
			repo,
			oid
		);			
					
		git_merge_options merge_opts = GIT_MERGE_OPTIONS_INIT;		
		merge_opts.file_favor = merge_file_favor;
		merge_opts.file_flags = 
			(GIT_MERGE_FILE_STYLE_DIFF3 | GIT_MERGE_FILE_DIFF_MINIMAL);

		git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
		checkout_opts.checkout_strategy = (
			GIT_CHECKOUT_SAFE |
			GIT_CHECKOUT_ALLOW_CONFLICTS |
			GIT_CHECKOUT_CONFLICT_STYLE_DIFF3
		);
		
		const git_annotated_commit *mergeheads[] = 
			{fetchhead_annotated_commit};

		git_merge(repo,mergeheads,
			1,
			&merge_opts,
			&checkout_opts);
				
		git_merge_analysis_t analysis;
		git_merge_preference_t preference = GIT_MERGE_PREFERENCE_NONE;
		git_merge_analysis(&analysis,
				&preference,
				repo,
				mergeheads
				,1);
		
		git_annotated_commit_free(fetchhead_annotated_commit);		

		if(analysis==GIT_MERGE_ANALYSIS_NORMAL) {		
			printf("Normal merge\n");
			git_signature * signature;
			git_signature_default(&signature,repo);
						
			git_oid commit_oid,oid_parent_commit,tree_oid;
			
			git_commit * parent_commit;
			
			git_commit * fetchhead_commit;

			git_commit_lookup(&fetchhead_commit,
				repo,
				oid
			);						
									
			git_reference_name_to_id( &oid_parent_commit, repo, "HEAD" );			
			git_commit_lookup( &parent_commit, repo, &oid_parent_commit );
			
			git_tree *tree;
			git_index *index;	
			
			git_repository_index(&index, repo);
			if(git_index_has_conflicts(index)) {
				printf("Index has conflicts\n");

				git_index_conflict_iterator *conflicts;
				const git_index_entry *ancestor;
				const git_index_entry *our;
				const git_index_entry *their;
				int err = 0;

				git_index_conflict_iterator_new(&conflicts, index);

				while ((err = git_index_conflict_next(&ancestor, &our, &their, conflicts)) == 0) {
					fprintf(stderr, "conflict: a:%s o:%s t:%s\n",
							ancestor ? ancestor->path : "NULL",
							our->path ? our->path : "NULL",
							their->path ? their->path : "NULL");
				}

				if (err != GIT_ITEROVER) {
					fprintf(stderr, "error iterating conflicts\n");
				}

				git_index_conflict_iterator_free(conflicts);
				
			} else {
				printf("No conflicts\n");
				git_index_write_tree(&tree_oid, index);
				git_tree_lookup(&tree, repo, &tree_oid);
						
				git_commit_create_v(
					&commit_oid,
					repo,
					"HEAD",
					signature,
					signature,
					NULL,
					"Merge with remote",
					tree,
					2, 
					parent_commit, 
					fetchhead_commit
				);
				
				git_repository_state_cleanup(repo);
			}
			
			git_index_free(index);					
			git_commit_free(parent_commit);
			git_commit_free(fetchhead_commit);			
		} else if(analysis==(GIT_MERGE_ANALYSIS_NORMAL | GIT_MERGE_ANALYSIS_FASTFORWARD)) {
			printf("Fast forward\n");
			git_reference * ref = NULL;		

			git_reference_lookup(&ref, repo, "refs/heads/master");
			git_reference *newref;
			git_reference_set_target(&newref,ref,oid,"pull");

			git_reference_free(newref);
			git_reference_free(ref);

			git_repository_state_cleanup(repo);
		} else if(analysis==GIT_MERGE_ANALYSIS_UP_TO_DATE) {
			printf("All up to date\n");
			git_repository_state_cleanup(repo);
		} else {
			printf("Don't know how to merge\n");
		}
												
		printf("Merged %s\n",remote_url);
	}	
	return 0;
}	

void EMSCRIPTEN_KEEPALIVE jsgitsetuser(const char *name, const char *email) {
	git_config *config;
	
	git_repository_config(&config, repo);
	git_config_set_string(config, "user.name", name);
	git_config_set_string(config, "user.email", email);
	git_config_free(config);
}

void EMSCRIPTEN_KEEPALIVE jsgitresolvemergecommit() {	
	git_index *index;
	git_repository_index(&index, repo);	
	
	git_oid commit_oid, tree_oid, oid_parent_commit, oid_fetchhead_commit;
	git_tree * tree;
	
	git_index_write_tree(&tree_oid, index);
	git_tree_lookup(&tree, repo, &tree_oid);

	git_signature * signature;
	git_signature_default(&signature,repo);
			
	git_commit * parent_commit;	
	git_commit * fetchhead_commit;
	
	git_reference_name_to_id( &oid_parent_commit, repo, "HEAD" );
	git_commit_lookup( &parent_commit, repo, &oid_parent_commit );

	git_reference_name_to_id( &oid_fetchhead_commit, repo, "FETCH_HEAD" );			
	git_commit_lookup( &fetchhead_commit, repo, &oid_fetchhead_commit );	
	
	git_commit_create_v(
		&commit_oid,
		repo,
		"HEAD",
		signature,
		signature,
		NULL,
		"Resolved conflicts and merge with remote",
		tree,
		2, 
		parent_commit,
		fetchhead_commit
	);	

	git_repository_state_cleanup(repo);
	git_signature_free(signature);
	git_index_free(index);		
	git_tree_free(tree);			
	git_commit_free(parent_commit);
	git_commit_free(fetchhead_commit);			
}

void EMSCRIPTEN_KEEPALIVE jsgitpull(int file_favor) {
	merge_file_favor = file_favor;
	
	git_remote *remote = NULL;
	const git_transfer_progress *stats;
	git_fetch_options fetch_opts = GIT_FETCH_OPTIONS_INIT;
	
	if (git_remote_lookup(&remote, repo, "origin") < 0)
		goto on_error;

	/* Set up the callbacks (only update_tips for now) */
	fetch_opts.callbacks.update_tips = &update_cb;
	fetch_opts.callbacks.sideband_progress = &progress_cb;
	fetch_opts.callbacks.transfer_progress = transfer_progress_cb;
	fetch_opts.callbacks.credentials = cred_acquire_cb;

	git_strarray headers;
	int has_headers = fetch_applyheaders(&headers);
	if(has_headers) {
		fetch_opts.custom_headers = headers;
	}

	/**
	 * Perform the fetch with the configured refspecs from the
	 * config. Update the reflog for the updated references with
	 * "fetch".
	 */	 
	int error = git_remote_fetch(remote, NULL, &fetch_opts, "fetch");
	if(has_headers) {
		fetch_freeheaders(&headers);
	}

	if(error < 0) {
		goto on_error;
	}

	/**
	 * If there are local objects (we got a thin pack), then tell
	 * the user how many objects we saved from having to cross the
	 * network.
	 */
	stats = git_remote_stats(remote);
	if (stats->local_objects > 0) {
		printf("\nReceived %d/%d objects in %" PRIuZ " bytes (used %d local objects)\n",
		       stats->indexed_objects, stats->total_objects, stats->received_bytes, stats->local_objects);
	} else{
		printf("\nReceived %d/%d objects in %" PRIuZ "bytes\n",
			stats->indexed_objects, stats->total_objects, stats->received_bytes);
	}

	printf("Fetch done\n");
		
	git_repository_fetchhead_foreach(repo,&fetchead_foreach_cb,NULL);
	
	//git_repository_state_cleanup(repo);

	printf("Pull done\n");
		
	return;

on_error:
	printLastError();

	git_remote_free(remote);	
	return;
}

int diff_file_cb(const git_diff_delta *delta, float progress, void *payload) {
	printf("Adding %s\n",delta->old_file.path);
	jsgitadd(delta->old_file.path);
	return 0;
}

void EMSCRIPTEN_KEEPALIVE jsgitaddfileswithchanges() {
	git_diff *diff;
	
	git_diff_index_to_workdir(&diff, repo, NULL, NULL);
	git_diff_foreach(diff,&diff_file_cb,NULL,NULL,NULL,NULL);

	git_diff_free(diff);
}

int EMSCRIPTEN_KEEPALIVE jsgitworkdirnumberofdeltas() {
	git_diff *diff;
	
	git_diff_index_to_workdir(&diff, repo, NULL, NULL);
	int ret = git_diff_num_deltas(diff);
	git_diff_free(diff);
	return ret;
}

int EMSCRIPTEN_KEEPALIVE jsgitstatus() {
	git_status_list *status;
	git_status_options statusopt = GIT_STATUS_OPTIONS_INIT;
	EM_ASM(
		jsgitstatusresult = [];
	);
	statusopt.show  = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
	statusopt.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED |
		GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX |
		GIT_STATUS_OPT_SORT_CASE_SENSITIVELY;
	git_status_list_new(&status, repo, &statusopt);

	size_t i, maxi = git_status_list_entrycount(status);
	const git_status_entry *s;
	int header = 0, changes_in_index = 0;
	int changed_in_workdir = 0, rm_in_workdir = 0;
	const char *old_path, *new_path;

	/** Print index changes. */

	for (i = 0; i < maxi; ++i) {
		char *istatus = NULL;

		s = git_status_byindex(status, i);

		if (s->status == GIT_STATUS_CURRENT)
			continue;

		if (s->status & GIT_STATUS_WT_DELETED)
			rm_in_workdir = 1;

		if (s->status & GIT_STATUS_INDEX_NEW)
			istatus = "new file: ";
		if (s->status & GIT_STATUS_INDEX_MODIFIED)
			istatus = "modified: ";
		if (s->status & GIT_STATUS_INDEX_DELETED)
			istatus = "deleted:  ";
		if (s->status & GIT_STATUS_INDEX_RENAMED)
			istatus = "renamed:  ";
		if (s->status & GIT_STATUS_INDEX_TYPECHANGE)
			istatus = "typechange:";

		if (istatus == NULL)
			continue;

		if (!header) {
			printf("# Changes to be committed:\n");
			printf("#   (use \"git reset HEAD <file>...\" to unstage)\n");
			printf("#\n");
			header = 1;
		}

		old_path = s->head_to_index->old_file.path;
		new_path = s->head_to_index->new_file.path;

		if (old_path && new_path && strcmp(old_path, new_path)) {
			printf("#\t%s  %s -> %s\n", istatus, old_path, new_path);
			EM_ASM_({
				jsgitstatusresult.push({
					old_path: UTF8ToString($0),
					new_path: UTF8ToString($1),
					status: UTF8ToString($2).trim().replace(':', '')
				});
			}, old_path, new_path, istatus);
		}
		else
		{
			printf("#\t%s  %s\n", istatus, old_path ? old_path : new_path);
			EM_ASM_({
				jsgitstatusresult.push({
					path: UTF8ToString($0),
					status: UTF8ToString($1).trim().replace(':', '')
				});
			}, old_path ? old_path : new_path, istatus);
		}
	}

	if (header) {
		changes_in_index = 1;
		printf("#\n");
	}
	header = 0;

	/** Print workdir changes to tracked files. */

	for (i = 0; i < maxi; ++i) {
		char *wstatus = NULL;

		s = git_status_byindex(status, i);

		/**
		 * With `GIT_STATUS_OPT_INCLUDE_UNMODIFIED` (not used in this example)
		 * `index_to_workdir` may not be `NULL` even if there are
		 * no differences, in which case it will be a `GIT_DELTA_UNMODIFIED`.
		 */
		if (s->status == GIT_STATUS_CURRENT || s->index_to_workdir == NULL)
			continue;

		/** Print out the output since we know the file has some changes */
		if (s->status & GIT_STATUS_WT_MODIFIED)
			wstatus = "modified: ";
		if (s->status & GIT_STATUS_WT_DELETED)
			wstatus = "deleted:  ";
		if (s->status & GIT_STATUS_WT_RENAMED)
			wstatus = "renamed:  ";
		if (s->status & GIT_STATUS_WT_TYPECHANGE)
			wstatus = "typechange:";

		if (wstatus == NULL)
			continue;

		if (!header) {
			printf("# Changes not staged for commit:\n");
			printf("#   (use \"git add%s <file>...\" to update what will be committed)\n", rm_in_workdir ? "/rm" : "");
			printf("#   (use \"git checkout -- <file>...\" to discard changes in working directory)\n");
			printf("#\n");
			header = 1;
		}

		old_path = s->index_to_workdir->old_file.path;
		new_path = s->index_to_workdir->new_file.path;

		if (old_path && new_path && strcmp(old_path, new_path)) {
			printf("#\t%s  %s -> %s\n", wstatus, old_path, new_path);
			EM_ASM_({
				jsgitstatusresult.push({
					old_path: UTF8ToString($0),
					new_path: UTF8ToString($1),
					status: UTF8ToString($2).trim().replace(':', '')
				});
			}, old_path, new_path, wstatus);
		} else {
			printf("#\t%s  %s\n", wstatus, old_path ? old_path : new_path);
			EM_ASM_({
				jsgitstatusresult.push({
					path: UTF8ToString($0),
					status: UTF8ToString($1).trim().replace(':', '')
				});
			}, old_path ? old_path : new_path, wstatus);
		}
	}

	if (header) {
		changed_in_workdir = 1;
		printf("#\n");
	}

	/** Print untracked files. */

	header = 0;

	for (i = 0; i < maxi; ++i) {
		s = git_status_byindex(status, i);

		if (s->status == GIT_STATUS_WT_NEW) {

			if (!header) {
				printf("# Untracked files:\n");
				printf("#   (use \"git add <file>...\" to include in what will be committed)\n");
				printf("#\n");
				header = 1;
			}

			printf("#\t%s\n", s->index_to_workdir->old_file.path);

			EM_ASM_({
				jsgitstatusresult.push({
					path: UTF8ToString($0),
					status: UTF8ToString($1).trim().replace(':', '')
				});
			}, s->index_to_workdir->old_file.path, "untracked");
		}
	}

	header = 0;

	/** Print ignored files. */

	for (i = 0; i < maxi; ++i) {
		s = git_status_byindex(status, i);

		if (s->status == GIT_STATUS_IGNORED) {

			if (!header) {
				printf("# Ignored files:\n");
				printf("#   (use \"git add -f <file>...\" to include in what will be committed)\n");
				printf("#\n");
				header = 1;
			}

			printf("#\t%s\n", s->index_to_workdir->old_file.path);
		}
	}

	git_index *index;	
	git_repository_index(&index, repo);	
	if(git_index_has_conflicts(index)) {
		printf("Index has conflicts\n");

		git_index_conflict_iterator *conflicts;
		const git_index_entry *ancestor;
		const git_index_entry *our;
		const git_index_entry *their;
		int err = 0;

		git_index_conflict_iterator_new(&conflicts, index);

		while ((err = git_index_conflict_next(&ancestor, &our, &their, conflicts)) == 0) {
			git_blob *our_blob;
			git_blob_lookup(&our_blob, repo, &our->id);

			git_blob *their_blob;
			git_blob_lookup(&their_blob, repo, &their->id);
			
			int is_binary = (our_blob != NULL && git_blob_is_binary(our_blob)) ||
					(their_blob != NULL && git_blob_is_binary(their_blob)) ? 1 : 0;

			fprintf(stderr, "conflict: a:%s o:%s t:%s, binary:%d\n",
					ancestor ? ancestor->path : "NULL",
					our->path ? our->path : "NULL",
					their->path ? their->path : "NULL", is_binary);
			
			EM_ASM_({
				jsgitstatusresult.push({
						ancestor: UTF8ToString($0),
						our: UTF8ToString($1),
						their: UTF8ToString($2),
						status: 'conflict',
						binary: $3
					});
				}, ancestor ? ancestor->path : "NULL",
						our->path ? our->path : "NULL",
						their->path ? their->path : "NULL",
						is_binary
			);
		}

		if (err != GIT_ITEROVER) {
			fprintf(stderr, "error iterating conflicts\n");
		}

		git_index_conflict_iterator_free(conflicts);
	}
	git_index_free(index);
	
	if (!changes_in_index && changed_in_workdir) {
		printf("no changes added to commit (use \"git add\" and/or \"git commit -a\")\n");
		return 0;
	} else {
		return 1;
	}
}

void EMSCRIPTEN_KEEPALIVE jsgitpush() {
	// get the remote.
	int error;

	git_remote* remote = NULL;
	git_remote_lookup( &remote, repo, "origin" );

	char *refspec = "refs/heads/master";
	const git_strarray refspecs = {
		&refspec,
		1,
	};

	// configure options
	git_push_options options;
	git_push_init_options( &options, GIT_PUSH_OPTIONS_VERSION );

	git_strarray headers;
	int has_headers = fetch_applyheaders(&headers);
	if(has_headers) {
		options.custom_headers = headers;
	}

	// do the push
	
	error = git_remote_push( remote, &refspecs, &options);

	// Free allocated strings for headers
	if(has_headers) {
		fetch_freeheaders(&headers);
	}
	
	if (error != 0) {
		const git_error *err = git_error_last();
		if (err) printf("ERROR %d: %s\n", err->klass, err->message);
		else printf("ERROR %d: no detailed info\n", error);
	}
}

void EMSCRIPTEN_KEEPALIVE jsgitreset_hard(char * committish) {
	git_object *obj = NULL;
	int error = git_revparse_single(&obj, repo, committish);
	
	if(error != 0) {
		const git_error *err = git_error_last();
		printf("ERROR %d: %s\n", err->klass, err->message);		
	}

	git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
	checkout_opts.checkout_strategy = GIT_CHECKOUT_FORCE;
	
	git_reset(repo,obj,GIT_RESET_HARD,&checkout_opts);
}

void jsfilter_free(git_filter *f)
{
	git__free(f);
}

int jsfilter_apply(
	git_filter     *self,
	void          **payload,
	git_buf        *to,
	const git_buf  *from,
	const git_filter_source *source)
{
	const unsigned char *src = (const unsigned char *)from->ptr;

	int filterresultsize = EM_ASM_INT({
		const buf = new Uint8Array(Module.HEAPU8.buffer,$3,$4);
		const filterfunctionkey = UTF8ToString($1);

		if(jsgitfilterfunctions[filterfunctionkey]) {
			jsgitfilterresult = jsgitfilterfunctions[filterfunctionkey](
				UTF8ToString($0),
				UTF8ToString($1),
				$2,
				buf
			);
		} else {
			/* console.log('Could not find filter',
				filterfunctionkey, 'for path',
				UTF8ToString($0),
				'attrs',
				UTF8ToString($1),
				'mode', $2); */
			jsgitfilterresult = buf;
		}

		return jsgitfilterresult.length;		
	}, git_filter_source_path(source), 
		self->attributes, 
		git_filter_source_mode(source), 
		src, from->size);
	
	unsigned char *dst;	
	
	git_buf_grow(to, filterresultsize);

	dst = (unsigned char *)to->ptr;
	to->size = filterresultsize;

	EM_ASM_({
		writeArrayToMemory(jsgitfilterresult, $0);		
	}, dst);

	return 0;
}

void EMSCRIPTEN_KEEPALIVE jsgitregisterfilter(char * name, char * attributes, int priority) {
	git_filter *filter = git__calloc(1, sizeof(git_filter));
	
	filter->version = GIT_FILTER_VERSION;
	filter->attributes = attributes;
	filter->shutdown = jsfilter_free; 
	filter->apply = jsfilter_apply;
	
	git_filter_register(name, filter, priority);
}

int EMSCRIPTEN_KEEPALIVE jsgitgetlasterror() {
	const git_error *err = git_error_last();
	
	EM_ASM_({
		jsgitlasterrorresult = ({
			klass: $0,
			message: UTF8ToString($1)
		});
	}, err->klass, err->message);
	return err->klass;
}