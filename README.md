libgit2 - the Git linkable library
======================

libgit2 is a portable, pure C implementation of the Git core methods provided as a
re-entrant linkable library with a solid API, allowing you to write native
speed custom Git applications in any language with bindings.

libgit2 is licensed under a **very permissive license** (GPLv2 with a special Linking Exception).
This basically means that you can link it (unmodified) with any kind of software without having to
release its source code.

* Mailing list: <libgit2@librelist.org>
* Website: <http://libgit2.github.com>
* API documentation: <http://libgit2.github.com/libgit2>
* Usage guide: <http://libgit2.github.com/api.html>

What It Can Do
==================================

libgit2 is already very usable.

* SHA conversions, formatting and shortening
* abstracked ODB backend system
* commit, tag, tree and blob parsing, editing, and write-back
* tree traversal
* revision walking
* index file (staging area) manipulation
* reference management (including packed references)
* config file management
* high level repository management
* thread safety and reentrancy
* descriptive and detailed error messages
* ...and more (over 175 different API calls)

Building libgit2 - Using CMake
==============================

libgit2 builds cleanly on most platforms without any external dependencies.
Under Unix-like systems, like Linux, \*BSD and Mac OS X, libgit2 expects `pthreads` to be available;
they should be installed by default on all systems. Under Windows, libgit2 uses the native Windows API
for threading.

The libgit2 library is built using CMake 2.6+ (<http://www.cmake.org>) on all platforms.

On most systems you can build the library using the following commands

	$ mkdir build && cd build
	$ cmake ..
	$ cmake --build .

Alternatively you can point the CMake GUI tool to the CMakeLists.txt file and generate platform specific build project or IDE workspace.

To install the library you can specify the install prefix by setting:

	$ cmake .. -DCMAKE_INSTALL_PREFIX=/install/prefix
	$ cmake --build . --target install

If you want to build a universal binary for Mac OS X, CMake sets it
all up for you if you use `-DCMAKE_OSX_ARCHITECTURES="i386;x86_64"`
when configuring.

For more advanced use or questions about CMake please read <http://www.cmake.org/Wiki/CMake_FAQ>.

The following CMake variables are declared:

- `INSTALL_BIN`: Where to install binaries to.
- `INSTALL_LIB`: Where to install libraries to.
- `INSTALL_INC`: Where to install headers to.
- `BUILD_SHARED_LIBS`: Build libgit2 as a Shared Library (defaults to ON)
- `BUILD_TESTS`: Build the libgit2 test suite (defaults to ON)
- `THREADSAFE`: Build libgit2 with threading support (defaults to OFF)

Language Bindings
==================================

Here are the bindings to libgit2 that are currently available:

* Rugged (Ruby bindings) <https://github.com/libgit2/rugged>
* objective-git (Objective-C bindings) <https://github.com/libgit2/objective-git>
* pygit2 (Python bindings) <https://github.com/libgit2/pygit2>
* libgit2sharp (.NET bindings) <https://github.com/libgit2/libgit2sharp>
* php-git (PHP bindings) <https://github.com/libgit2/php-git>
* luagit2 (Lua bindings) <https://github.com/libgit2/luagit2>
* GitForDelphi (Delphi bindings) <https://github.com/libgit2/GitForDelphi>
* node-gitteh (Node.js bindings) <https://github.com/libgit2/node-gitteh>
* nodegit (Node.js bindings) <https://github.com/tbranyen/nodegit>
* go-git (Go bindings) <https://github.com/str1ngs/go-git>
* libqgit2 (C++ QT bindings) <https://projects.kde.org/projects/playground/libs/libqgit2/>
* libgit2-ocaml (ocaml bindings) <https://github.com/burdges/libgit2-ocaml>
* Geef (Erlang bindings) <https://github.com/schacon/geef>
* libgit2net (.NET bindings, low level) <https://github.com/txdv/libgit2net>
* parrot-libgit2 (Parrot Virtual Machine bindings) <https://github.com/letolabs/parrot-libgit2>
* hgit2 (Haskell bindings) <https://github.com/norm2782/hgit2>

If you start another language binding to libgit2, please let us know so
we can add it to the list.

How Can I Contribute
==================================

Fork libgit2/libgit2 on GitHub, add your improvement, push it to a branch
in your fork named for the topic, send a pull request.

You can also file bugs or feature requests under the libgit2 project on
GitHub, or join us on the mailing list by sending an email to:

libgit2@librelist.com


License
==================================
libgit2 is under GPL2 **with linking exemption**. This means you
can link to the library with any program, commercial, open source or
other.  However, you cannot modify libgit2 and distribute it without
supplying the source.

See the COPYING file for the full license text.
