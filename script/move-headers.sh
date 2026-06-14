#!/bin/sh

for h in src/util/*.h; do

  git mv $h include/git2
  git commit -m "$(basename $h): move to git2/$(basename $h)"
  rg -l "\"$(basename $h)\"" | xargs sed -i "s|\"$(basename $h)\"|\"git2/$(basename $h)\"|"
  git commit -a --amend --no-edit

done
