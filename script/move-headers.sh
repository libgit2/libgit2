#!/bin/sh

set -x

# fixme why does this stop looping?
# find src/util/ -name '*.h' | while read path; do
for path in $(find src/util/ -name '*.h'); do

  relpath=${path:9} # remove the "src/util/" prefix
  echo "relpath: $relpath"
  dir=$(dirname $path)
  reldir=$(dirname $relpath)
  name=$(basename $path)
  mkdir -p include/git2/$reldir
  git mv $path include/git2/$reldir
  git commit -m "$relpath: move to git2/$relpath"
  # no, this produces false-positive matches
  # rg -l "\"($name|$relpath)\"" | xargs -r sed -i "s|\"$name\"|\"git2/$relpath\"|; s|\"$relpath\"|\"git2/$relpath\"|"
  rg -l "\"$relpath\"" | xargs -r sed -i "s|\"$relpath\"|\"git2/$relpath\"|"
  git commit -a --amend --no-edit

done
