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
* API documentation: <http://libgit2.github.com/libgit2/modules.html>
* Usage guide: <http://libgit2.github.com/api.html>

What It Can Do
==================================

libgit2 is already very usable.

* SHA conversions, formatting and shortening
* object reading (loose and packed)
* object writing (loose)
* commit, tag, tree and blob parsing and write-back
* tree traversal
* revision walking
* index file (staging area) manipulation
* custom ODB backends
* reference management (including packed references)
* ...and more


Building libgit2 - External dependencies
========================================

libgit2 builds cleanly on most platforms without any external dependencies.
Under Unix-like systems, like Linux, *BSD and Mac OS X, libgit2 expects `pthreads` to be available;
they should be installed by default on all systems. Under Windows, libgit2 uses the native Windows API
for threading.

Additionally, he following libraries may be used as replacement for built-in functionality:

* LibSSL **(optional)** <http://www.openssl.org/>

libgit2 can be built using the SHA1 implementation of LibSSL-Crypto, instead of the built-in custom implementations. Performance wise, they are quite similar.

Building libgit2 - Using waf
======================

Waf is a minimalist build system which only requires a Python 2.5+ interpreter to run. This is the default build system for libgit2.

To build libgit2 using waf, first configure the build system by running:

    $ ./waf configure

Then build the library, either in its shared (libgit2.so) or static form (libgit2.a):

    $ ./waf build-static
    $ ./waf build-shared

You can then run the full test suite with:

    $ ./waf test

And finally you can install the library with (you may need to sudo):

    $ sudo ./waf install

The waf build system for libgit2 accepts the following flags:

	--debug
		build the library with debug symbols.
		Defaults to off.

	--sha1=[builtin|ppc|openssl]
		use the builtin SHA1 functions, the optimized PPC versions
		or the SHA1 functions from LibCrypto (OpenSSL).
		Defaults to 'builtin'.

	--msvc=[7.1|8.0|9.0|10.0]
		Force a specific version of the MSVC compiler, if more than
		one version is installed.

	--arch=[ia64|x64|x86|x86_amd64|x86_ia64]
		Force a specific architecture for compilers that support it.

	--with-sqlite
		Enable sqlite support.

You can run `./waf --help` to see a full list of install options and
targets.


Building libgit2 - Using CMake
==============================

The libgit2 library can also be built using CMake 2.6+ (<http://www.cmake.org>) on all platforms.

On most systems you can build the library using the following commands

	$ mkdir build && cd build
	$ cmake ..
	$ cmake --build .

Alternatively you can point the CMake GUI tool to the CMakeLists.txt file and generate platform specific build project or IDE workspace.

To install the library you can specify the install prefix by setting:

	$ cmake .. -DCMAKE_INSTALL_PREFIX=/install/prefix
	$ cmake --build . --target install

For more advanced use or questions about CMake please read <http://www.cmake.org/Wiki/CMake_FAQ>.


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
