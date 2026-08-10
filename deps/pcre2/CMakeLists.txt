#
# Static configuration
#

set(SUPPORT_PCRE2_8 1)
set(SUPPORT_UNICODE 1)
set(PCRE2_NEWLINE "LF")
set(NEWLINE_DEFAULT "2")
set(PCRE2_LINK_SIZE "2")
set(PCRE2_MAX_VARLOOKBEHIND "255")
set(PCRE2_PARENS_NEST_LIMIT "250")
set(PCRE2_HEAP_LIMIT "20000000")
set(PCRE2_MATCH_LIMIT "10000000")
set(PCRE2_MATCH_LIMIT_DEPTH "MATCH_LIMIT")


#
# Dynamic configuration
#

include(CheckCSourceCompiles)
include(CheckFunctionExists)
include(CheckSymbolExists)
include(CheckIncludeFile)

check_include_file(assert.h HAVE_ASSERT_H)
check_include_file(dirent.h HAVE_DIRENT_H)
check_include_file(sys/stat.h HAVE_SYS_STAT_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(unistd.h HAVE_UNISTD_H)
check_include_file(windows.h HAVE_WINDOWS_H)

check_symbol_exists(memfd_create "sys/mman.h" HAVE_MEMFD_CREATE) # glibc 2.27
check_symbol_exists(secure_getenv "stdlib.h" HAVE_SECURE_GETENV) # glibc 2.17

check_c_source_compiles(
  "int main(void) { char buf[128] __attribute__((uninitialized)); (void)buf; return 0; }"
  HAVE_ATTRIBUTE_UNINITIALIZED
)

check_c_source_compiles("int main(void) { __assume(1); return 0; }" HAVE_BUILTIN_ASSUME)

check_c_source_compiles(
  [=[
  #include <stddef.h>
  int main(void) { int a,b; size_t m; __builtin_mul_overflow(a,b,&m); return 0; }
  ]=]
  HAVE_BUILTIN_MUL_OVERFLOW
)

check_c_source_compiles(
  "int main(int c, char *v[]) { if (c) __builtin_unreachable(); return (int)(*v[0]); }"
  HAVE_BUILTIN_UNREACHABLE
)


# Output files

configure_file(config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h @ONLY)


# Source code

set(PCRE2_HEADERS pcre2.h)
set(PCRE2_SOURCES
  pcre2_auto_possess.c
  pcre2_chartables.c
  pcre2_chkdint.c
  pcre2_compile.c
  pcre2_compile_cgroup.c
  pcre2_compile_class.c
  pcre2_config.c
  pcre2_context.c
  pcre2_convert.c
  pcre2_dfa_match.c
  pcre2_error.c
  pcre2_extuni.c
  pcre2_find_bracket.c
  pcre2_maketables.c
  pcre2_match.c
  pcre2_match_data.c
  pcre2_match_next.c
  pcre2_newline.c
  pcre2_ord2utf.c
  pcre2_pattern_info.c
  pcre2_script_run.c
  pcre2_serialize.c
  pcre2_string_utils.c
  pcre2_study.c
  pcre2_substitute.c
  pcre2_substring.c
  pcre2_tables.c
  pcre2_ucd.c
  pcre2_valid_utf.c
  pcre2_xclass.c
)


# Build setup

add_definitions(-DHAVE_CONFIG_H)
add_definitions(-DPCRE2_CODE_UNIT_WIDTH=8)

if(MSVC)
  add_compile_definitions(_CRT_SECURE_NO_DEPRECATE _CRT_SECURE_NO_WARNINGS)
endif()

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_library(pcre2 OBJECT ${PCRE2_HEADERS} ${PCRE2_SOURCES})
