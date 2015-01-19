#include "common.h"
#include <git2.h>
#include <git2/checkout.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
# include <pthread.h>
# include <unistd.h>
#endif


typedef struct progress_data {
    size_t completed_steps;
    size_t total_steps;
    const char *path;
} progress_data;


static void print_progress(const progress_data *pd)
{
    int checkout_percent = pd->total_steps
                           ? (100 * pd->completed_steps / pd->total_steps)
                           : 100;

    printf( "chk %3d%% (%4" PRIuZ "/%4" PRIuZ ") %s\n",
            checkout_percent,
            pd->completed_steps, pd->total_steps,
            pd->path );
}


static void checkout_progress(const char *path, size_t cur, size_t tot, void *payload)
{
    progress_data *pd = (progress_data*)payload;
    pd->completed_steps = cur;
    pd->total_steps = tot;
    pd->path = path;
    print_progress(pd);
}


int do_checkout_ref(git_repository *repo, git_reference *checkout_ref)
{
    progress_data pd = {0};
    git_object *target_tree = NULL;
    git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;

    int error;

    // Set up options
    checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE | GIT_CHECKOUT_ALLOW_CONFLICTS;
    checkout_opts.progress_cb = checkout_progress;
    checkout_opts.progress_payload = &pd;

    // Get the tree for checkout
    error = git_reference_peel( &target_tree, checkout_ref, GIT_OBJ_TREE );
    if ( error ) {
        goto error;
    }

    // Do the checkout
    error = git_checkout_tree( repo, target_tree, &checkout_opts );
    if ( error ) {
        goto error;
    }

    // Update HEAD to point to the reference
    error = git_repository_set_head( repo, git_reference_name( checkout_ref ), NULL, NULL );

error:
    printf("\n");
    if ( error ) {
        const git_error *err = giterr_last();
        if (err) printf("ERROR %d: %s\n", err->klass, err->message);
        else printf("ERROR %d: no detailed info\n", error);
    }

    git_object_free( target_tree );
    git_reference_free( checkout_ref );

    return error;
}


int main(int argc, char **argv)
{
        git_repository *repo = NULL;

        if (argc != 1 || argv[1] /* silence -Wunused-parameter */)
                fatal("Sorry, no options supported yet", NULL);

        // TODO: lookup a provided reference name in the repo
        git_reference  *ref = NULL; // provide a valid reference

        check_lg2(git_repository_open(&repo, "."),
                  "Could not open repository", NULL);
        check_lg2(do_checkout_ref( repo, ref ),
                  "Could not checkout reference", NULL);

        return 0;
}
