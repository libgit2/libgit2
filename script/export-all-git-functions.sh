#!/bin/sh

rg -l '^extern [a-z].+ git_[a-z_]+\(' include/git2/ | xargs sed -i -E 's/^extern ([a-z].+) (git_[a-z_]+)\(/GIT_EXTERN(\1) \2(/'

rg -l '^extern [a-z].+ \*git_[a-z_]+\(' include/git2/ | xargs sed -i -E 's/^extern ([a-z].+ \*)(git_[a-z_]+)\(/GIT_EXTERN(\1) \2(/'
