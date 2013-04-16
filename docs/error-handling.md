Error reporting in libgit2
==========================

Error reporting is performed on an explicit `git_error **` argument, which appears at the end of all API calls that can return an error. Yes, this does clutter the API.

When a function fails, an error is set on the error variable **and** returns one of the generic error codes.

~~~c
int git_repository_open(git_repository **repository, const char *path, git_error **error)
{
	// perform some opening
	if (p_exists(path) < 0) {
		giterr_set(error, GITERR_REPOSITORY, "The path '%s' doesn't exist", path);
		return GIT_ENOTFOUND;
	}

	...

	if (try_to_parse(path, error) < 0)
		return GIT_ERROR;

	...
}
~~~

The simple error API
--------------------

- `void giterr_set(git_error **, int, const char *, ...)`: the main function used to set an error. It allocates a new error object and stores it in the passed error pointer. It has no return value. The arguments for `giterr_set` are as follows:

	- `git_error **error_ptr`: the pointer where the error will be created.
	- `int error_class`: the class for the error. This is **not** an error code: this is an specific enum that specifies the error family. The point is to map these families 1-1 with Exception types on higher level languages (e.g. GitRepositoryException)
	- `const char *error_str, ...`: the error string, with optional formatting arguments

- `void giterr_free(git_error *)`: takes an error and frees it. This function is available in the external API.

- `void giterr_clear(git_error **)`: clears an error previously set in an error pointer, setting it to NULL and calling `giterr_free` on it.

- `void giterr_propagate(git_error **, git_error *)`: moves an error to a given error pointer, handling the case when the error pointer is NULL (in that case the error gets freed, because it cannot be propagated).

The new error code return values
--------------------------------

We are doing this the POSIX way: one error code for each "expected failure", and a generic error code for all the critical failures.

For instance: A reference lookup can have an expected failure (which is when the reference cannot be found), and a critical failure (which could be any of a long list of things that could go wrong, such as the refs packfile being corrupted, a loose ref being written with the wrong permissions, etc). We cannot have distinct error codes for every single error in the library, hence `git_reference_lookup` would return GIT_SUCCESS if the operation was successful, GIT_ENOTFOUND when the reference doesn't exist, and GIT_ERROR when an error happens -- **the error is then detailed in the `git_error` parameter**.

Please be smart when returning error codes. Functions have max two "expected errors", and in most cases only one.

Writing error messages
----------------------

Here are some guidelines when writing error messages:

- Use proper English, and an impersonal or past tenses: *The given path does not exist*, *Failed to lookup object in ODB*

- Use short, direct and objective messages. **One line, max**. libgit2 is a low level library: think that all the messages reported will be thrown as Ruby or Python exceptions. Think how long are common exception messages in those languages.

- **Do not add redundant information to the error message**, specially information that can be inferred from the context.

	E.g. in `git_repository_open`, do not report a message like "Failed to open repository: path not found". Somebody is
	calling that function. If it fails, he already knows that the repository failed to open!

General guidelines for error reporting
--------------------------------------

- We never handle programming errors with these functions. Programming errors are `assert`ed, and when their source is internal, fixed as soon as possible. This is C, people.

	Example of programming errors that would **not** be handled: passing NULL to a function that expects a valid pointer; passing a `git_tree` to a function that expects a `git_commit`. All these cases need to be identified with `assert` and fixed asap.

	Example of a runtime error: failing to parse a `git_tree` because it contains invalid data. Failing to open a file because it doesn't exist on disk. These errors would be handled, and a `git_error` would be set.

- The `git_error **` argument is always the last in the signature of all API calls. No exceptions.

- When the programmer (or us, internally) doesn't need error handling, he can pass `NULL` to the `git_error **` param. This means that the errors won't be *reported*, but obviously they still will be handled (i.e. the failing function will interrupt and return cleanly). This is transparently handled by `giterr_set`

- `git_error *` **must be initialized to `NULL` before passing its value to a function!!**

	~~~c
	git_error *err;
	git_error *good_error = NULL;

	git_foo_func(arg1, arg2, &error); // invalid: `error` is not initialized
	git_foo_func2(arg1, arg2, &good_error); // OK!
	git_foo_func3(arg1, arg2, NULL); // OK! But no error reporting!
	~~~

- Piling up errors is an error! Don't do this! Errors must always be free'd when a function returns.

	~~~c
	git_error *error = NULL;

	git_foo_func1(arg1, &error);
	git_foo_func2(arg2, &error); // WRONG! What if func1 failed? `error` would leak!
	~~~

- Likewise: do not rethrow errors internally!

	~~~c
	int git_commit_create(..., git_error **error)
	{
		if (git_reference_exists("HEAD", error) < 0) {
			/* HEAD does not exist; create it so we can commit... */
			if (git_reference_create("HEAD", error) < 0) {
				/* error could be rethrown */
			}
		}

- Remember that errors are now allocated, and hence they need to be free'd after they've been used. Failure to do so internally (e.g. in the already seen examples of error piling) will be reported by Valgrind, so we can easily find where are we rethrowing errors.

- Remember that any function that fails **will set an error object**, and that object will be freed.
