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

    http://libgit2.github.com/libgit2

such as:

    http://libgit2.github.com/libgit2/ex/HEAD/general.html

Prerequisites for examples
--------------------------
1. [CMake](https://cmake.org/), and is recommended to be installed into your `PATH`.
2. `libgit2`, it is assumed you have already built [and installed](../README.md#installation), to the default system path (don't specify `CMAKE_INSTALL_PREFIX`).
3. [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/) is used by `examples/CMakeLists.txt` to determine the necessary include and link paths.  In this way, it is also an example of how an external project might include `libgit2` as a dependency using cmake.

Build
-----
Similarly to `libgit2` itself, the examples can be built with cmake using the following commands

        $ mkdir examples/build && cd examples/build
        $ cmake ..
        $ cmake --build .
