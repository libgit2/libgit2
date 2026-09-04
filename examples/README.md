libgit2 examples
================

These examples are a mixture of basic emulation of core Git command line
functions and simple snippets demonstrating libgit2 API usage (for use
with Docurium).  As a whole, they are not vetted carefully for bugs, error
handling, and cross-platform compatibility in the same manner as the rest
of the code in libgit2, so copy with caution.

That being said, you are welcome to copy code from these examples as
desired when using libgit2. They have been [released to the public domain][cc0],
so there are no restrictions on their use.

[cc0]: COPYING

For annotated HTML versions, see the "Examples" section of:

    https://libgit2.org/libgit2

such as:

    https://libgit2.org/libgit2/ex/HEAD/general.html

Partial clone and sparse checkout
---------------------------------

`lg2 partial-clone <url> <path> <directory>...` demonstrates a filtered
`blob:none` clone combined with cone-mode sparse checkout. Each directory is
relative to the repository root.

Build and run it from the repository root:

    cmake --build build/linux-x86_64-debug --target lg2
    build/linux-x86_64-debug/examples/lg2 partial-clone \
        https://github.com/libgit2/libgit2.git libgit2-clone src/libgit2 tests

For authenticated remotes, the command uses the shared interactive credential
callback used by the other `lg2` network examples.
