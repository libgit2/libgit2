#include <iostream>
#include "git2.h"

#define MAX_PATHSPEC 8

struct opts
{
    git_status_options statusopt;
    char *repodir;
    char *pathspec[MAX_PATHSPEC];
    int npaths;
    int format;
    int zterm;
    int showbranch;
    int showsubmod;
    int repeat;
};

int main()
{
    git_repository *repo;
    struct opts o = {GIT_STATUS_OPTIONS_INIT, "."};

    int error = 0;
    const char *branch = NULL;
    git_reference *head = NULL;

    error = git_repository_head(&head, repo);

    if (error == GIT_EUNBORNBRANCH || error == GIT_ENOTFOUND)
        branch = NULL;
    else if (!error)
    {
        branch = git_reference_shorthand(head);
    }
    else
    {
        std::cout << "failed to get current branch: " << error << std::endl;
        git_reference_free(head);
        exit(1);
    }
    git_reference_free(head);
    exit(0);
}
