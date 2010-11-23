libgit2 - the Git linkable library
==================================

libgit2 is a portable, pure C implementation of the Git core methods provided as a
re-entrant linkable library with a solid API, allowing you to write native
speed custom Git applications in any language with bindings.

Why Do We Need It
==================================

In the current Git project, though a libgit.a file is produced it is
not re-entrant (it will call <code>die()</code> on basically any error)
and it has no stable or well-designed public API.  As there is no good
way to link to this effectively, a new library was needed that fulfilled
these requirements.  Thus libgit2.

Though it would be nice to use the same library that Git itself uses, 
Git actually has a pretty simple storage format and just having native
access to that is pretty useful.  Eventually we would like to have most
of the functionality of the core Git tools or even get the library
integrated into Git itself, but in the meantime having a cleanly designed
and maintained linkable Git library with a public API will likely be helpful
to lots of people.

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

Building libgit2 - Unix systems
==================================

In Unix-like systems, like Linux, *BSD and Mac OS X, libgit2 has
the following dependencies:

	- Python 2.5-3.1 <http://www.python.org>
		Used by run the build system; no extra libraries required.
		Should probably ship installed with your OS.

	- zlib 1.2+ <http://www.zlib.net/>

	- LibSSL <http://www.openssl.org/>
		Only needed if you want to re-use OpenSSL's SHA1 routines;
		libgit2 compiles its own routines by default.

To build it, first configure the build system by running:

  $ ./waf configure

Then build the library, either in its shared (libgit2.so) or static form (libgit2.a)

  $ ./waf build-static
  $ ./waf build-shared

You can then test the library with:

  $ ./waf test

And finally you can install it with (you may need to sudo):

  $ ./waf install

Building libgit2 - Windows MSVC++
==================================

When building under Windows using the MSVC compiler, libgit2 has
the following dependencies:

	- Python 2.5-3.1 <http://www.python.org>
		Used by run the build system; no extra libraries required.

	- zlib 1.2+ (Windows API Version) <http://www.zlib.net/>
		Make sure you compile the ZLib library using the MSVC solution
		that ships in its source distribution.
		Alternatively, you may download precompiled binaries from:
			http://www.winimage.com/zLibDll/

	- LibSSL <http://www.openssl.org/>
		Only needed if you want to re-use OpenSSL's SHA1 routines;
		libgit2 compiles its own routines by default.

To build it, first configure the build system by running:

  $ ./waf configure

Then build the library, either in its shared (libgit2.dll) or static form (libgit2.lib)

  $ ./waf build-static
  $ ./waf build-shared

You can then test the library with:

  $ ./waf test

Lastly, you can manually install the generated *.lib and *.dll files, depending on
your preferences.

Building libgit2 - Windows MinGW
==================================

When building under Windows using the GCC compiler that ships with MinGW,
libgit2 has the following dependencies:

	- Python 2.5-3.1 <http://www.python.org>
		Used by run the build system; no extra libraries required.

	- zlib 1.2+ <http://www.zlib.net/>

	- pthreads-w32 <http://sourceware.org/pthreads-win32/>
		Or an equivalent pthreads implementation for non-POSIX systems

	- LibSSL <http://www.openssl.org/>
		Only needed if you want to re-use OpenSSL's SHA1 routines;
		libgit2 compiles its own routines by default.

To build it, first configure the build system and force GCC as the compiler,
instead of the default MSVC:

  $ ./waf configure --check-c-compiler=gcc

Then build the library, either in its shared (libgit2.so) or static form (libgit2.a)

  $ ./waf build-static
  $ ./waf build-shared

You can then test the library with:

  $ ./waf test

And finally you can install it with:

  $ ./waf install


Configuration settings
==================================

The waf build system for libgit2 accepts the following flags:

	--debug
		build the library with debug symbols.
		Defaults to off.

	--sha1=[builtin|ppc|openssl]
		use the builtin SHA1 functions, the optimized PPC versions
		or the SHA1 functions from LibCrypto (OpenSSL).
		Defaults to 'builtin'.

You can run `./waf --help` to see a full list of install options and
targets.

Language Bindings
==================================

So you want to use Git from your favorite programming language.  Here are
the bindings to libgit2 that are currently available:

== Ruby ==

Ribbit is the reference library used to make sure the 
libgit2 API is sane.  This should be mostly up to date.

http://github.com/libgit2/ribbit


== Erlang ==

Geef is an example of an Erlang NIF binding to libgit2.  A bit out of 
date, but basically works.  Best as a proof of concept of what you could
do with Erlang and NIFs with libgit2.

http://github.com/schacon/geef


If you start another language binding to libgit2, please let us know so
we can add it to the list.

License 
==================================
libgit2 is under GPL2 with linking exemption, which basically means you
can link to the library with any program, commercial, open source or
other.  However, you cannot modify libgit2 and distribute it without
supplying the source.

See the COPYING file for the full license text.
