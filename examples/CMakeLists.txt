# examples: code usage examples of libgit2
cmake_minimum_required(VERSION 3.5.1)
project(libgit2examples LANGUAGES C)

set(SRC_EXAMPLES 
  add.c
  args.c
  args.h
  blame.c
  cat-file.c
  checkout.c
  clone.c
  commit.c
  common.c
  common.h
  config.c
  describe.c
  diff.c
  fetch.c
  for-each-ref.c
  general.c
  index-pack.c
  init.c
  lg2.c
  log.c
  ls-files.c
  ls-remote.c
  merge.c
  push.c
  remote.c
  rev-list.c
  rev-parse.c
  show-index.c
  stash.c
  status.c
  tag.c
)

add_executable(lg2 ${SRC_EXAMPLES})
set_target_properties(lg2 PROPERTIES C_STANDARD 90)

# Ensure that we do not use deprecated functions internally
target_compile_definitions(lg2 PRIVATE GIT_DEPRECATE_HARD)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBGIT2 REQUIRED libgit2)
target_include_directories(lg2 SYSTEM PRIVATE ${LIBGIT2_INCLUDE_DIRS})
target_link_libraries(lg2 ${LIBGIT2_LINK_LIBRARIES})
