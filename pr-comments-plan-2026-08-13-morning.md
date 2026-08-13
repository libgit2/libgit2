# Plan: respond to review on PR #7338

Edward Thomson left six inline comments on [PR #7338](https://github.com/libgit2/libgit2/pull/7338) on 2026-08-13.
The review state is `COMMENTED`, with no summary body and no other outstanding review feedback.

**This document is a plan.
Do not implement it until asked.**

## Decisions

The comments require five changes and one no-change response.
None requires push-back or clarification.

| ID | Location | Decision |
| --- | --- | --- |
| **C1** | `docs/changelog.md:1` | Remove the unreleased changelog entry. |
| **C2** | `transports/bundle.c:65` | Replace the probe enum with a `bool` result. |
| **C3** | `transport.c:35` | Use `bundle://` as the fake lookup key so registration can override content-probed bundles. |
| **C4** | `transports/bundle.c:42` | Make no change; the reviewer explicitly excluded the shared helper from this PR. |
| **C5** | `transports/bundle.c:81` | Correct the explanation for the regular-file check. |
| **C6** | `transports/bundle.c:94` | Put the parsed header under the bundle parser lifecycle. |

Four points that previously generated unnecessary alternatives are settled, each by the reviewer's own wording. Quote the wording rather than re-deriving the conclusion:

- **C2 is a bool out-param, not a bool return.**
  The reviewer wrote the signature himself: "Should this just be `probe(bool *out...)`?"
  An out-param with an `int` return is what S2 specifies and is the pervasive libgit2 convention, so the "plain `bool` return, all failures become misses" alternative is contradicted by the request that prompted it.
- **C3 sanctions a fake definition.**
  The reviewer wrote: "We should have a protocol definition here, even if it's fake, so that callers can override it meaningfully."
  The stated purpose is override capability, which registration lookup satisfies; "even if it's fake" rules out committing to a user-facing scheme. `bundle://` stays an internal dispatch key and is not added to `transports[]`.
  His trailing "If this was `bundle://` would that be a problem?" is a direct question, so the reply must answer it explicitly.
- **C5 is comment-only, and the reviewer is right.**
  He wrote: "Would it? Isn't that just another form of 'no'?"
  A directory is already a probe miss via `!S_ISREG` at `src/libgit2/transports/bundle.c:84`, so the existing comment's claim that treating it as an error "would break every ordinary local clone" is simply false. Fix the words; change no behavior.
- **The parser owns the parsed header.**
  Keeping extracted header fields directly on the transport would preserve the caller-owned result model that C6 asks to replace.
  See "Precedent for C6" below for the citations.

## The probe's error policy is not under review (root cause of the churn)

This is the single largest source of oscillation in this document's history, so it is recorded separately from the points above. Those are settled by what the reviewer *wrote*; this one is settled by what he did **not** write.

**No comment asks about it.** The six comments cover the changelog, the probe enum, the protocol definition, a local-path helper, a directory explanation, and the parser lifecycle. Whether an operational failure *after* regular-file classification — a failed `open`, `seek`, `read`, or allocation on a file that already looks like a bundle — should propagate as an error or degrade to a probe miss is raised by none of them.

That behavior is the PR's existing, deliberate design: a file carrying a bundle signature that cannot be read reports its `EACCES` rather than `unsupported URL protocol`. It ships unchanged.

Two traps this document has fallen into repeatedly, both of which end here:

1. **Re-opening it under C2.** "Collapse the probe to a `bool`" reads like a mandate to delete the error channel. It is not: the reviewer's own `probe(bool *out...)` keeps the `int` return.
2. **Re-opening it under C5.** Correcting a wrong comment about directories does not license a new probe-error policy. `!S_ISREG` already handles directories; nothing downstream of it is in scope.

A prior revision argued the policy was "not justified" because `network::remote::bundle::read_failure_propagates` was added by this branch (`e1990af88`) and so asserts the policy rather than establishing it. That observation is true and irrelevant — an unreviewed design decision does not become an open question merely because its own test came with it. **Do not re-litigate this. If the reviewer raises it, that is the point at which it becomes a question.**

## Precedent for C6 (settled — do not re-derive)

This question has been re-analyzed several times. It is answered by the reviewer's wording plus three in-repo precedents. Record, don't re-litigate.

**The comment (`transports/bundle.c:94`):**
> Instead of having a heavy object that is owned by the caller, it seems like this should be part of the bundle parser lifecycle.

The objection is specifically to *caller ownership of a heavy object*. At the anchored site the caller declares and disposes **two** objects — `git_bundle_reader reader` and `git_bundle_header header` (`src/libgit2/transports/bundle.c:67-68`), torn down by a matched pair at lines 93-94.

**Every other parsed file format in libgit2 exposes one owning object with one disposal entry point:**

| Type | Open | Free | Owns |
| --- | --- | --- | --- |
| `git_commit_graph_file` (`commit_graph.h:30-90`) | `git_commit_graph_file_open` (`:149`) | `git_commit_graph_file_free` (`:205`) | `git_map graph_map` + parsed tables |
| `git_midx_file` (`midx.h`) | `git_midx_open` (`:96`) | `git_midx_free` (`:113`) | `git_map index_map`, `packfile_names` + parsed tables |
| `git_patch_parsed` (`patch_parse.c:15-34`) | `git_patch_parse` | `patch_parsed__free` (`:1149`) | refcounted `git_patch_parse_ctx *` |

In none of these does the caller hold a separate reader/context lifecycle alongside the result. **This is what settles C6: fold the reader and header into one parser with one dispose.** That is S6.

**Why S6 also clears borrowed source state, consistent with the same precedents.**
Those three retain their source because their parsed data has a *live dependency* on it — commit-graph and midx store `const unsigned char *` pointers **into** `git_map`, and `git_patch_parsed` reads `patch->ctx->opts.prefix_len` during `check_prefix`. The bundle header has no such dependency: ref names are `git__strdup`'d (`src/libgit2/bundle.c:346`) and prerequisites are a `git_array_t(git_oid)` of values, so the parsed header is self-contained the moment parsing returns.

Two consequences, both already reflected in S6:

1. Clearing borrowed source state on every parse exit path is correct, not a deviation.
2. The earlier objection that a retained parser makes the transport *heavier* than today's `git_bundle_header` is **unfounded** — after clearing, the retained parser is a `git_bundle_header` plus zeroed fields. Do not raise it again.

## Implementation

### S1 — Drop the changelog entry (C1)

Remove the 15-line `Unreleased` block added by this branch from `docs/changelog.md`.
Leave all released entries unchanged.

### S2 — Return the probe result through a bool (C2)

Change the probe signature to:

```c
int git_transport_bundle__probe(bool *out, const char *url);
```

Delete `git_bundle_probe_t` and the three `GIT_BUNDLE_PROBE_*` constants.
Initialize `*out` to `false`.

Preserve the existing classification and error channels:

| Probe outcome | Return | `*out` |
| --- | --- | --- |
| Supported bundle | `0` | `true` |
| Well-formed bundle with unsupported semantics | `0` | `true` |
| Not a local path, missing path, initial `stat` failure, or non-regular file | `0` | `false` |
| Invalid or malformed bundle header | `0` | `false` |
| Allocation, open, seek, or read failure after regular-file classification | negative | `false` |

A recognized but unsupported bundle remains `true` so `bundle_connect` can report the specific unsupported feature instead of `unsupported URL protocol`.
Keep the `int` return and propagate operational failures at both probe call sites.

### S3 — Use `bundle://` as the fake override key (C3)

Give the internal bundle definition the requested lookup prefix:

```c
static transport_definition bundle_transport_definition =
	{ "bundle://", git_transport_bundle, NULL };
```

Do not add this definition to `transports[]` and do not add bundle URL parsing.
The string is only the key used to find a custom transport after content probing succeeds.

At both positive probe sites in `transport_find_fn`, look up `bundle_transport_definition.prefix` with `transport_find_by_url` and fall back to `&bundle_transport_definition` when no custom definition matches.
This lets `git_transport_register("bundle", ...)` override the built-in bundle transport because registration stores the prefix `bundle://`.

Preserve these dispatch properties:

- A registered `bundle` transport wins for content-probed bundles at both probe call sites.
- The custom transport receives the original filesystem path through the existing connection flow.
- A drive-rooted Windows bundle is probed before the colon-based SSH fallback.
- On Unix, a path containing a colon continues to select SSH first.
- An unregistered literal `bundle://` URL remains unsupported.

Add focused tests to `tests/libgit2/transport/register.c` using the existing `dummy_transport`:

1. Registration for `bundle` overrides a content-probed relative bundle path.
2. Registration for `bundle` overrides an absolute fixture path, exercising the drive-rooted probe on Windows.

The second test is a Windows-specific dispatch witness and may be guarded or documented accordingly.
Extend suite cleanup to unregister `bundle` and remove any copied fixture.
Keep `test_transport_register__custom_ssh_precedes_colon_bundle_path` unchanged.

Do not add a recording transport merely to assert that `connect` receives the original path.
The lookup returns only the factory and parameter; it has no path by which the fake key could replace the caller's URL.

### S4 — Leave the local-path helper alone (C4)

Make no code change.
The reviewer explicitly said this PR need not add the helper.

### S5 — Correct the regular-file comment (C5)

Replace the incorrect claim that a directory read failure would break ordinary local clones:

```c
	/*
	 * Only regular files can be bundles. Avoid opening special files,
	 * which may block or have side effects.
	 */
	if (p_stat(url, &st) < 0 || !S_ISREG(st.st_mode))
		return 0;
```

A directory is already a probe miss through `S_ISREG`.
Do not change dispatch order or broaden this comment fix into a new probe-error policy.
Open and read failures after regular-file classification continue to propagate.
Keep `network::remote::bundle::read_failure_propagates` as the regression witness.

### S6 — Make the parser own its parsed header (C6)

Replace the separate caller-owned `git_bundle_reader` and `git_bundle_header` lifecycles with one parser that owns the parsed result.
Keep `git_bundle_header` as a named nested result because transport code and prerequisite checking address its fields, but make its disposal private to `bundle.c`.

Use this internal shape:

```c
typedef struct {
	/* borrowed source and transient parsing state */
	git_bundle_read_cb read;
	git_bundle_seek_cb seek;
	void *payload;
	int fd;
	const unsigned char *data;
	size_t data_len;
	size_t data_pos;
	git_str buf;
	size_t offset;
	unsigned int eof : 1;

	/* parsed result, owned by the parser */
	git_bundle_header header;
} git_bundle_parser;

#define GIT_BUNDLE_PARSER_INIT { 0 }

int git_bundle_parser_parse(git_bundle_parser *parser);
int git_bundle_parser_parse_fd(git_bundle_parser *parser, int fd);
int git_bundle_parser_parse_memory(
	git_bundle_parser *parser, const void *data, size_t len);
void git_bundle_parser_dispose(git_bundle_parser *parser);
```

Remove `GIT_BUNDLE_HEADER_INIT`, the caller-facing `git_bundle_header_dispose`, and the separate reader lifecycle.
Implement all three parse entry points through one common parse-and-cleanup path.
`git_bundle_parser_parse` consumes callback source fields already installed by its caller; the descriptor and memory entry points install their backing and then enter that same path.

Parser ownership rules:

- Zero-initialization creates a valid parser.
- Each parse entry point releases any previous parsed result before starting a new parse.
- The parser borrows descriptors, memory, callback payloads, and never closes or frees them.
- Every parse return path disposes the transient read buffer and zeroes the borrowed source and transient fields.
- Source cleanup preserves the parse return code and the error describing an unsupported or failed parse.
- Successful and `GIT_ENOTSUPPORTED` parses leave their header owned by the parser until re-parsing or disposal.
- A hard parse failure releases partial refs and prerequisites.
- Disposal releases the parsed result and is idempotent.

Transport changes:

- Replace `transport_bundle.header` with `transport_bundle.parser`.
- Keep the transport's descriptor because pack download reads from it directly.
- Parse with `git_bundle_parser_parse_fd(&t->parser, t->fd)`.
- Read refs, prerequisites, object type, and pack offset from `t->parser.header`.
- Retain the parser after parsing so advertised refs remain available after disconnect.
- Closing a connection closes the descriptor but does not dispose the parser.
- Reset and free dispose the parser.
- Preserve the defensive SHA-1 default before the first connection.

The probe uses a short-lived parser and disposes it on every exit path.

Convert `tests/libgit2/bundle/parse.c` to the parser-owned result without changing its behavioral coverage.
Preserve memory-backed parsing, descriptor-backed parsing, custom read callbacks, seek-position checks, malformed and unsupported inputs, and failure cleanup.
Add focused tests that re-parsing releases the previous result and that disposal is idempotent.
Add one source-cleanup helper and exercise it after success, `GIT_ENOTSUPPORTED`, and a hard parse failure.
Assert that callback pointers, borrowed backing state, offsets, EOF state, and the transient buffer are cleared; that a descriptor remains open and caller-owned; and that failure cleanup preserves the return code and `git_error_last()`.
Keep `network::remote::bundle::refs_remain_available_after_disconnect` as the transport-level lifetime witness.

## Files expected to change

| File | Change |
| --- | --- |
| `docs/changelog.md` | Remove the branch changelog entry. |
| `src/libgit2/bundle.h` | Bool probe and parser-owned result API. |
| `src/libgit2/bundle.c` | Parser lifecycle, source cleanup, and private header cleanup. |
| `src/libgit2/transport.c` | Bool probe and fake override lookup. |
| `src/libgit2/transports/bundle.c` | Probe comment, bool result, and retained parser. |
| `tests/libgit2/bundle/parse.c` | Parser lifecycle conversion and ownership tests. |
| `tests/libgit2/transport/register.c` | Bundle-registration override tests. |

Do not add a literal `bundle://` path parser, a shared local-path helper, an original-path recording transport, or transport-dispatch reordering.

## Verification

Build and run the directly affected suites:

```sh
cmake --build build -j8
./build/libgit2_tests -sbundle::parse -sclone::bundle -sfetch::bundle \
                      -snetwork::remote::bundle -stransport::register
```

Run the broader consumers and the full suite:

```sh
./build/libgit2_tests -sclone -sfetch -snetwork::remote
./build/libgit2_tests
```

Run the ownership-sensitive suites under the host's leak checker:

```sh
# Linux
./script/valgrind.sh ./build/libgit2_tests \
    -sbundle::parse -snetwork::remote::bundle

# macOS
./script/leaks.sh ./build/libgit2_tests \
    -sbundle::parse -snetwork::remote::bundle
```

If neither checker is available, use a separately configured ASan/LSan build and record the configuration.

Run final static checks:

```sh
git diff upstream/main...HEAD -- docs/changelog.md
rg -n "git_bundle_probe_t|GIT_BUNDLE_PROBE_|GIT_BUNDLE_HEADER_INIT" src tests
rg -n "git_bundle_header_dispose" src/libgit2/bundle.h src/libgit2/transports tests
rg -n "git_bundle_reader_fromfd|git_bundle_reader_frommemory" src tests
git diff --check
```

These commands must produce no output.
Push only after local verification succeeds and CI passes.
If a failure is claimed to be inherited from `main`, record the compared `main` run or commit.

## Delivery hygiene

The local commits ahead of `origin/git-bundles-clone-fetch` are revisions of this planning document, not PR content.
Before pushing implementation, transplant or consolidate the implementation so `pr-comments-plan-2026-08-13-morning.md` and the plan-only commits are absent from the prospective PR.

Verify that explicitly:

```sh
git diff --name-only upstream/main...HEAD
git log --oneline origin/git-bundles-clone-fetch..HEAD
```

## Success criteria

1. The PR contains no changelog or planning-document change.
2. Probe callers consume a `bool`, while operational failures still propagate.
3. Supported and recognized-but-unsupported bundles both select a bundle transport.
4. Invalid files, directories, special files, and initial `stat` failures remain probe misses.
5. A registered `bundle` transport overrides content-probed paths at both probe call sites.
6. Literal `bundle://` behavior and colon-path precedence remain unchanged.
7. One parser owns and frees parsed refs and prerequisites, including after partial failures and re-parsing.
8. Parsing does not retain borrowed source state or transient buffers after returning.
9. Advertised refs remain available after disconnect.
10. The focused suites, broader suites, leak check, full suite, and CI pass without unexplained regressions.

## Proposed review replies

- **C1 — changelog:** Dropped.

- **C2 — probe result:** Changed the result to `bool`.
  Recognized but unsupported bundles still return `true` so `connect` can report the specific unsupported feature, and the `int` return remains for operational failures.

- **C3 — protocol definition:** Changed the fake definition to `bundle://` and routed positive content probes through that lookup, so `git_transport_register("bundle", ...)` can override the built-in transport.
  It remains an internal dispatch key; this does not add a user-facing `bundle://` URL scheme.

- **C4 — local-path helper:** Agreed; no change here per your note.

- **C5 — directory probe:** You're right.
  A directory is already a probe miss through the regular-file check.
  Reworded the comment to explain that the guard avoids opening special files that may block or have side effects.

- **C6 — parser lifecycle:** Folded the reader and parsed header into one parser-owned lifecycle.
  Parsing releases borrowed source state and transient buffers before returning, while parser disposal releases the parsed refs and prerequisites.
  The transport retains the parser so advertised refs remain available after disconnect.
