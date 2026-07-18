#!/bin/sh

decl_footer=$'\n/** @} */\nGIT_END_DECL\n\n#endif'

# include/git2/blob.h
decl_footer_2=$'\n/** @} */\nGIT_END_DECL\n#endif'

find include/git2/ -name '*.h' | while read path; do

  actual_footer="$(tail -c32 $path)"
  [ "$actual_footer" = "$decl_footer" ] && continue

  actual_footer_2="$(tail -c31 $path)"
  [ "$actual_footer_2" = "$decl_footer_2" ] && continue

  last_line=$(tail -n1 $path)
  if [ "$last_line" != "#endif" ]; then
    echo "FIXME $path does not end with '#endif'"
    # some files have an extra empty line after '#endif'
    # include/git2/attr.h
    # some files have the include guard inside another if block
    # include/git2/stdint.h
    continue
  fi
  # continue # debug

  echo "$path: ${actual_footer@Q}"
  {
    # remove last line
    cat $path | head -n-1
    # add decl footer without the leading '\n'
    echo "${decl_footer:1}"
  } | sponge $path

done
