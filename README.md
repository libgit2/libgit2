libgit2 - the Git linkable library
======================

[![Build Status](https://secure.travis-ci.org/libgit2/libgit2.png?branch=development)](http://travis-ci.org/libgit2/libgit2)

libgit2 is a portable, pure C implementation of the Git core methods provided as a
re-entrant linkable library with a solid API, allowing you to write native
speed custom Git applications in any language with bindings.

libgit2 is licensed under a **very permissive license** (GPLv2 with a special Linking Exception).
This basically means that you can link it (unmodified) with any kind of software without having to
release its source code.

* Mailing list: ~~<libgit2@librelist.org>~~
    The libgit2 mailing list has
    traditionally been hosted in Librelist, but Librelist is and has always
    been a shitshow. We encourage you to [open an issue](https://github.com/libgit2/libgit2/issues)
    on GitHub instead for any questions regarding the library.
    * Archives: <http://librelist.com/browser/libgit2/>
* Website: <http://libgit2.github.com>
* API documentation: <http://libgit2.github.com/libgit2>

What It Can Do
==================================

libgit2 is already very usable.

* SHA conversions, formatting and shortening
* abstracted ODB backend system
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

For more advanced use or questions about CMake please read <http://www.cmake.org/Wiki/CMake_FAQ>.

The following CMake variables are declared:

- `BIN_INSTALL_DIR`: Where to install binaries to.
- `LIB_INSTALL_DIR`: Where to install libraries to.
- `INCLUDE_INSTALL_DIR`: Where to install headers to.
- `BUILD_SHARED_LIBS`: Build libgit2 as a Shared Library (defaults to ON)
- `BUILD_CLAR`: Build [Clar](https://github.com/vmg/clar)-based test suite (defaults to ON)
- `THREADSAFE`: Build libgit2 with threading support (defaults to OFF)
- `STDCALL`: Build libgit2 as `stdcall`. Turn off for `cdecl` (Windows; defaults to ON)

Compiler and linker options
---------------------------

CMake lets you specify a few variables to control the behavior of the
compiler and linker. These flags are rarely used but can be useful for
64-bit to 32-bit cross-compilation.

- `CMAKE_C_FLAGS`: Set your own compiler flags
- `CMAKE_FIND_ROOT_PATH`: Override the search path for libraries
- `ZLIB_LIBRARY`, `OPENSSL_SSL_LIBRARY` AND `OPENSSL_CRYPTO_LIBRARY`:
Tell CMake where to find those specific libraries

MacOS X
-------

If you want to build a universal binary for Mac OS X, CMake sets it
all up for you if you use `-DCMAKE_OSX_ARCHITECTURES="i386;x86_64"`
when configuring.

Windows
-------

You need to run the CMake commands from the Visual Studio command
prompt, not the regular or Windows SDK one. Select the right generator
for your version with the `-G "Visual Studio X" option.

See [the wiki]
(https://github.com/libgit2/libgit2/wiki/Building-libgit2-on-Windows)
for more detailed instructions.

Language Bindings
==================================

Here are the bindings to libgit2 that are currently available:

* C++
    * libqgit2, Qt bindings <https://projects.kde.org/projects/playground/libs/libqgit2/>
* Chicken Scheme
    * chicken-git <https://wiki.call-cc.org/egg/git>
* D
    * dlibgit <https://github.com/AndrejMitrovic/dlibgit>
* Delphi
    * GitForDelphi <https://github.com/libgit2/GitForDelphi>
* Erlang
    * Geef <https://github.com/schacon/geef>
* Go
    * go-git <https://github.com/str1ngs/go-git>
* GObject
    * libgit2-glib <https://live.gnome.org/Libgit2-glib>
* Haskell
    * hgit2 <https://github.com/norm2782/hgit2>
* Lua
    * luagit2 <https://github.com/libgit2/luagit2>
* .NET
    * libgit2net, low level bindings <https://github.com/txdv/libgit2net>
    * libgit2sharp <https://github.com/libgit2/libgit2sharp>
* Node.js
    * node-gitteh <https://github.com/libgit2/node-gitteh>
    * nodegit <https://github.com/tbranyen/nodegit>
* Objective-C
    * objective-git <https://github.com/libgit2/objective-git>
* OCaml
    * libgit2-ocaml <https://github.com/burdges/libgit2-ocaml>
* Parrot Virtual Machine
    * parrot-libgit2 <https://github.com/letolabs/parrot-libgit2>
* Perl
    * git-xs-pm <https://github.com/ingydotnet/git-xs-pm>
* PHP
    * php-git <https://github.com/libgit2/php-git>
* Python
    * pygit2 <https://github.com/libgit2/pygit2>
* Ruby
    * Rugged <https://github.com/libgit2/rugged>
* Vala
    * libgit2.vapi <https://github.com/apmasell/vapis/blob/master/libgit2.vapi>

If you start another language binding to libgit2, please let us know so
we can add it to the list.

How Can I Contribute?
==================================

Fork libgit2/libgit2 on GitHub, add your improvement, push it to a branch
in your fork named for the topic, send a pull request. If you change the
API or make other large changes, make a note of it in docs/rel-notes/ in a
file named after the next release.

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
