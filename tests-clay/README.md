Writing Clay tests for libgit2
==============================

For information on the Clay testing framework and a detailed introduction
please visit:

https://github.com/tanoku/clay


* Write your modules and tests. Use good, meaningful names.

* Make sure you actually build the tests by setting:

        cmake -DBUILD_CLAY=ON build/

* Test:

        ./build/libgit2_clay

* Make sure everything is fine.

* Send your pull request. That's it.
