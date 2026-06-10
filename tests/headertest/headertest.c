/*
 * Dummy project to validate header files
 *
 * This project is not intended to be executed, it should only include all
 * header files to make sure that they can be used with stricter compiler
 * settings than the libgit2 source files generally supports.
 */
#include "git2.h"

/*
 * `git2/sys/stream.h` is not included by `git2.h`, but it is a public
 * header that must be consumable on its own; in particular, on Win32 it
 * is responsible for providing `ssize_t` for its callbacks.
 */
#include "git2/sys/stream.h"

int main(void)
{
    return 0;
}
