libgit2 - the Git linkable library
======================

libgit2 is a portable, pure C implementation of the Git core methods provided as a
re-entrant linkable library with a solid API, allowing you to write native
speed custom Git applications in any language with bindings.

* Website: <http://libgit2.github.com>
* API documentation: <http://libgit2.github.com/libgit2/modules.html>
* Usage guide: <http://libgit2.github.com/api.html>

What It Can Do
==================================

libgit2 is already very usable.

* raw <-> hex SHA conversions
* raw object reading (loose and packed)
* raw object writing (loose)
* revlist walker
* commit, tag and tree object parsing and write-back
* tree traversal
* basic index file (staging area) operations

Building libgit2 - External dependencies
========================================

The following libraries are required to manually build the libgit2 library:

* zlib 1.2+ <http://www.zlib.net/>

When building in Windows using MSVC, make sure you compile ZLib using the MSVC solution that ships in its source distribution.
Alternatively, you may download precompiled binaries from: <http://www.winimage.com/zLibDll/>

* LibSSL **(optional)** <http://www.openssl.org/>

libgit2 can be built using the SHA1 implementation of LibSSL-Crypto, instead of the built-in custom implementations. Performance wise, they are quite similar.

* pthreads-w32 **(required on MinGW)** <http://sourceware.org/pthreads-win32/>

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

	--without-sqlite
		Disable sqlite support.

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
* pygit2 (Python bindings) <https://github.com/libgit2/pygit2>
* libgit2sharp (.NET bindings) <https://github.com/nulltoken/libgit2sharp>
* php-git (PHP bindings) <https://github.com/chobie/php-git>
* luagit2 (Lua bindings) <https://github.com/Neopallium/luagit2>
* GitForDelphi (Delphi bindings) <https://github.com/jasonpenny/GitForDelphi>
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
