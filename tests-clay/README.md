Writing Clay tests for libgit2
==============================

For information on the Clay testing framework and a detailed introduction
please visit:

https://github.com/tanoku/clay


* Write your modules and tests. Use good, meaningful names.

* Mix the tests:

        ./clay -vtap .

* Make sure you actually build the tests by setting:

        BUILD_CLAY=ON

* Test:

        ./build/libgit2_clay

* Make sure everything is fine.

* Send your pull request. That's it.
