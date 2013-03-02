# Welcome to libgit2!

We're making it easy to do interesting things with git, and we'd love to have
your help.

## Discussion & Chat

We hang out in the #libgit2 channel on irc.freenode.net.

Also, feel free to open an
[Issue](https://github.com/libgit2/libgit2/issues/new) to start a discussion
about any concerns you have.  We like to use Issues for that so there is an
easily accessible permanent record of the conversation.

## Reporting Bugs

First, know which version of libgit2 your problem is in and include it in
your bug report.  This can either be a tag (e.g.
[v0.17.0](https://github.com/libgit2/libgit2/tree/v0.17.0) ) or a commit
SHA (e.g.
[01be7863](https://github.com/libgit2/libgit2/commit/01be786319238fd6507a08316d1c265c1a89407f)
).  Using [`git describe`](http://git-scm.com/docs/git-describe) is a great
way to tell us what version you're working with.

If you're not running against the latest `development` branch version,
please compile and test against that to avoid re-reporting an issue that's
already been fixed.

It's *incredibly* helpful to be able to reproduce the problem.  Please
include a list of steps, a bit of code, and/or a zipped repository (if
possible).  Note that some of the libgit2 developers are employees of
GitHub, so if your repository is private, find us on IRC and we'll figure
out a way to help you.

## Pull Requests

Our work flow is a typical GitHub flow, where contributors fork the
[libgit2 repository](https://github.com/libgit2/libgit2), make their changes
on branch, and submit a
[Pull Request](https://help.github.com/articles/using-pull-requests)
(a.k.a. "PR").

Life will be a lot easier for you (and us) if you follow this pattern
(i.e. fork, named branch, submit PR).  If you use your fork's `development`
branch, things can get messy.

Please include a nice description of your changes with your PR; if we have
to read the whole diff to figure out why you're contributing in the first
place, you're less likely to get feedback and have your change merged in.

## Porting Code From Other Open-Source Projects

`libgit2` is licensed under the terms of the GPL v2 with a linking
exception.  Any code brought in must be compatible with those terms.

The most common case is porting code from core Git.  Git is a pure GPL
project, which means that in order to port code to this project, we need the
explicit permission of the author.  Check the
[`git.git-authors`](https://github.com/libgit2/libgit2/blob/development/git.git-authors)
file for authors who have already consented; feel free to add someone if
you've obtained their consent.

Other licenses have other requirements; check the license of the library
you're porting code *from* to see what you need to do.  As a general rule,
MIT and BSD (3-clause) licenses are typically no problem.  Apache 2.0
license typically doesn't work due to GPL incompatibility.

## Style Guide

`libgit2` is written in [ANSI C](http://en.wikipedia.org/wiki/ANSI_C)
(a.k.a. C89) with some specific conventions for function and type naming,
code formatting, and testing.

We like to keep the source code consistent and easy to read.  Maintaining
this takes some discipline, but it's been more than worth it.  Take a look
at the
[conventions file](https://github.com/libgit2/libgit2/blob/development/CONVENTIONS.md).

## Starter Projects

So, you want to start helping out with `libgit2`? That's fantastic? We
welcome contributions and we promise we'll try to be nice.

If you want to jump in, you can look at our issues list to see if there
are any unresolved issues to jump in on.  Also, here is a list of some
smaller project ideas that could help you become familiar with the code
base and make a nice first step:

* Convert a `git_*modulename*_foreach()` callback-based iteration API
  into a `git_*modulename*_iterator` object with a create/advance style
  of API.  This helps folks writing language bindings and usually isn't
  too complicated.
* Write a new `examples/` program that mirrors a particular core git
  command.  (See `examples/diff.c` for example.)  This lets you (and us)
  easily exercise a particular facet of the API and measure compatability
  and feature parity with core git.
* Submit a PR to clarify documentation! While we do try to document all of
  the APIs, your fresh eyes on the documentation will find areas that are
  confusing much more easily.
