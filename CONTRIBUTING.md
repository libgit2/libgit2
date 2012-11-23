# Welcome to libgit2!

We're making it easy to do interesting things with git, and we'd love to have
your help.

## Discussion & Chat

We hang out in the #libgit2 channel on irc.freenode.net.

## Reporting Bugs

First, know which version of libgit2 your problem is in.  Compile and test
against the `development` branch to avoid re-reporting an issue that's already
been fixed.

It's *incredibly* helpful to be able to reproduce the problem.  Please include
a bit of code and/or a zipped repository (if possible).  Note that some of the
developers are employees of GitHub, so if your repository is private, find us
on IRC and we'll figure out a way to help you.

## Pull Requests

Life will be a lot easier for you if you create a named branch for your
contribution, rather than just using your fork's `development`.

It's helpful if you include a nice description of your change with your PR; if
someone has to read the whole diff to figure out why you're contributing in the
first place, you're less likely to get feedback and have your change merged in.

## Porting Code From Other Open-Source Projects

The most common case here is porting code from core Git.  Git is a GPL project,
which means that in order to port code to this project, we need the explicit
permission of the author.  Check the
[`git.git-authors`](https://github.com/libgit2/libgit2/blob/development/git.git-authors)
file for authors who have already consented; feel free to add someone if you've
obtained their consent.

Other licenses have other requirements; check the license of the library you're
porting code *from* to see what you need to do.

## Styleguide

We like to keep the source code consistent and easy to read.  Maintaining this
takes some discipline, but it's been more than worth it.  Take a look at the
[conventions file](https://github.com/libgit2/libgit2/blob/development/CONVENTIONS.md).

