libgit2 - the Git linkable library
==================================

libgit2 is a portable, pure C implementation of the Git core methods provided as a
re-entrant linkable library with a solid API, allowing you to write native
speed custom Git applications in any language with bindings.

Installing libgit2
==================================

Libgit2 uses the waf build system.  To build it, first configure the build
system by running:

  $ ./waf configure

Then build the library:

  $ ./waf build

You can then test the library with:

  $ ./waf test

And finally you can install it with (you may need to sudo):

  $ ./waf install


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


Installing libgit2
==================================

 $ git clone git://github.com/libgit2/libgit2.git
 $ cd libgit2
 $ make
 $ make install

That should get it installed on Mac, Linux or Windows.
Once that is done, you should be able to link the library to your program
with a normal "-lgit2".

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
