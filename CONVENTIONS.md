# Libgit2 Conventions

We like to keep the source consistent and readable.  Herein are some guidelines
that should help with that.


## Naming Things

All types and functions start with `git_`, and all #define macros start with `GIT_`.

Functions with a single output parameter should name that parameter `out`.
Multiple outputs should be named `foo_out`, `bar_out`, etc.

Parameters of type `git_oid` should be named `id`, or `foo_id`.  Calls that
return an OID should be named `git_foo_id`.

Where there is a callback passed in, the `void *` that is passed into it should
be named "payload".

## Typedef

Wherever possible, use `typedef`.  If a structure is just a collection of
function pointers, the pointer types don't need to be separately typedef'd, but
loose function pointer types should be.

## Exports

All exported functions must be declared as:

```C
GIT_EXTERN(result_type) git_modulename_functionname(arg_list);
```

## Internals

Functions whose modulename is followed by two underscores,
for example `git_odb__read_packed`, are semi-private functions.
They are primarily intended for use within the library itself,
and may disappear or change their signature in a future release.

## Parameters

Out parameters come first.

Whenever possible, pass argument pointers as `const`.  Some structures (such
as `git_repository` and `git_index`) have internal structure that prevents
this.

Callbacks should always take a `void *` payload as their last parameter.
Callback pointers are grouped with their payloads, and come last when passed as
arguments:

```C
int foo(git_repository *repo, git_foo_cb callback, void *payload);
```


## Memory Ownership

Some APIs allocate memory which the caller is responsible for freeing; others
return a pointer into a buffer that's owned by some other object.  Make this
explicit in the documentation.


## Return codes

Return an `int` when a public API can fail in multiple ways.  These may be
transformed into exception types in some bindings, so returning a semantically
appropriate error code is important.  Check
[`errors.h`](https://github.com/libgit2/libgit2/blob/development/include/git2/errors.h)
for the return codes already defined.

Use `giterr_set` to provide extended error information to callers.

If an error is not to be propagated, use `giterr_clear` to prevent callers from
getting the wrong error message later on.


## Opaque Structs

Most types should be opaque, e.g.:

```C
typedef struct git_odb git_odb;
```

...with allocation functions returning an "instance" created within
the library, and not within the application.  This allows the type
to grow (or shrink) in size without rebuilding client code.

To preserve ABI compatibility, include an `int version` field in all opaque
structures, and initialize to the latest version in the construction call.
Increment the "latest" version whenever the structure changes, and try to only
append to the end of the structure.

## Option Structures

If a function's parameter count is too high, it may be desirable to package up
the options in a structure.  Make them transparent, include a version field,
and provide an initializer constant or constructor.  Using these structures
should be this easy:

```C
git_foo_options opts = GIT_FOO_OPTIONS_INIT;
opts.baz = BAZ_OPTION_ONE;
git_foo(&opts);
```

## Enumerations

Typedef all enumerated types.  If each option stands alone, use the enum type
for passing them as parameters; if they are flags, pass them as `unsigned int`.

## Code Layout

Try to keep lines less than 80 characters long.  Use common sense to wrap most
code lines; public function declarations should use this convention:

```C
GIT_EXTERN(int) git_foo_id(
   git_oid **out,
   int a,
   int b);
```

Indentation is done with tabs; set your editor's tab width to 3 for best effect.


## Documentation

All comments should conform to Doxygen "javadoc" style conventions for
formatting the public API documentation.  Try to document every parameter, and
keep the comments up to date if you change the parameter list.


## Public Header Template

Use this template when creating a new public header.

```C
#ifndef INCLUDE_git_${filename}_h__
#define INCLUDE_git_${filename}_h__

#include "git/common.h"

/**
 * @file git/${filename}.h
 * @brief Git some description
 * @defgroup git_${filename} some description routines
 * @ingroup Git
 * @{
 */
GIT_BEGIN_DECL

/* ... definitions ... */

/** @} */
GIT_END_DECL
#endif
```

## Inlined functions

All inlined functions must be declared as:

```C
GIT_INLINE(result_type) git_modulename_functionname(arg_list);
```

