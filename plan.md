# Bundle clone and fetch plan

## Decisions

- Start a fresh pull request from current `main` on `git-bundles-clone-fetch`.
- Limit the first pull request to reading local bundle files for clone and fetch plus the narrow generic clone-policy correction required for Git-compatible HEAD-less clones.
- Use PR #7101 by Laurence McGlashan as the implementation baseline, but do not preserve a known-broken mechanical port as a commit.
- Credit PR #7101 in commit messages and the pull request description with plain prose.
- Address the actionable review from PR #7221: do not add `bundle://`, and advertise `HEAD` first.
- Do not synthesize a missing `HEAD`; teach clone's existing no-`HEAD` path to select the configured initial branch only when that exact branch was advertised and fetched, matching Git while reusing libgit2's existing branch and tracking helpers.
- Verify prerequisites by checking that each prerequisite object exists in the destination object database, matching Git 2.50.1's observed behavior for valid repositories, and rely on thin-pack resolution to fail on genuinely missing bases. Do not build a graph-walking verifier.
- Prefer one pull request with three functional, independently green commits.
- Defer bundle creation, a public bundle API, and a fuzz target.
- Do not defer fuzzability: the header parser must be drivable from an in-memory buffer so a later fuzz target needs no redesign.

## Goal

Add a read-only local bundle transport that lets libgit2 clone a self-contained bundle and fetch a self-contained or incremental bundle.

The work is complete when libgit2 can:

- Recognize a valid bundle supplied as a plain local filesystem path, independent of its extension.
- Parse and advertise references from v2 and v3 bundle headers.
- Put a recorded `HEAD` first, leave a missing `HEAD` unsynthesized, and apply Git's exact initial-branch fallback in clone.
- Clone self-contained SHA-1 and SHA-256 bundles.
- Verify prerequisite existence before importing an incremental bundle pack.
- Resolve a thin pack against prerequisite objects in the destination object database.
- Reject an object-format mismatch before pack ingestion.
- Report transfer progress and honor cancellation.
- Reject malformed, unsupported, push, and bundle-download shallow cases without mutating references.

## Scope

The first pull request includes:

- An internal bundle-header parser.
- Internal prerequisite existence verification.
- A read-only bundle transport for reference listing, clone, and fetch.
- A small generic clone fallback for remotes that omit `HEAD`, implemented in clone policy rather than the bundle transport.
- Content-based detection for plain local file paths.
- SHA-1 and SHA-256 handling consistent with current libgit2 support.
- Static fixtures and focused parser, transport, clone, and fetch tests.
- A `docs/changelog.md` entry, as requested for major changes by `docs/contributing.md`.

The first pull request does not include:

- Bundle creation.
- A stable high-level `git_bundle_*` API.
- Push support.
- A `bundle://` URL scheme.
- Git's bundle-URI protocol.
- Filtered or promisor bundles.
- Shallow or deepen operations.
- Reading from standard input or arbitrary streams.
- A fuzz target and its corpus.
- A graph-walking prerequisite verifier beyond existence checks.

The low-level `git_transport_bundle` constructor may be exposed from `include/git2/sys/transport.h`, following the existing built-in transport convention.
Issues #1718 and #6824 also request bundle creation, so the pull request should reference them without claiming to close them.

## Sources and constraints

PR #7101 is the code baseline.
Its useful structure and tests should be adapted to current libgit2 conventions, then corrected as described below.

Implementation decisions may use:

- The published bundle-format documentation.
- Black-box behavior from the Git command-line client.
- Existing libgit2 transport, clone, fetch, ODB, and test conventions.
- Code contributed in PR #7101.

Do not copy or translate Git source without first auditing the relevant contributors against `git.git-authors`.
The plan does not require consulting Git source.

Relevant current-main facts already verified:

- `oid_type` is an unconditional transport callback (`include/git2/sys/transport.h:81`).
- Clone already applies the remote object format to the new repository (`src/libgit2/clone.c:448-449`).
- Clone and default-branch logic inspect the first advertised ref for `HEAD` (`src/libgit2/clone.c:229`, `src/libgit2/remote.c:2921`).
- `git_remote__default_branch` already falls back to guessing a branch when `HEAD` carries no symref target: it scans `refs/heads/*` for entries whose OID matches `HEAD` and prefers `git_repository_initialbranch` (`src/libgit2/remote.c:2932-2965`).
- When `HEAD` is absent, clone currently calls `update_head_to_default` without consulting advertised branches, so it leaves the configured initial branch unborn even when that branch was fetched (`src/libgit2/clone.c:225-230`, `src/libgit2/clone.c:140-163`).
- Clone's explicit-branch path already resolves the fetched remote-tracking ref and delegates local-branch creation and upstream configuration to `update_head_to_new_branch` (`src/libgit2/clone.c:260-301`); the no-`HEAD` fallback should reuse that path rather than duplicate it.
- `git_fetch_download_pack` calls `t->shallow_roots` unconditionally immediately after `t->download_pack` (`src/libgit2/fetch.c:212-213`), so a bundle transport that leaves the callback NULL crashes on every fetch.
- `git_fetch_negotiate` calls `t->negotiate_fetch` unconditionally when a pack is needed (`src/libgit2/fetch.c:194`).
- `git_fetch_negotiate` returns before the transport callback when every wanted tip already exists locally (`src/libgit2/fetch.c:178-180`).
- Writing an empty shallow-root array removes the destination's `shallow` file (`src/libgit2/repository.c:3946-3979`), so rejecting only a requested depth is not enough; a bundle download into an already-shallow repository must also be rejected.
- `git_odb_write_pack` can resolve thin-pack bases from the destination ODB (`include/git2/odb.h:498`).
- The local transport shows the existing progress-callback pattern (`src/libgit2/transports/local.c:640-690`), the mandatory no-op `shallow_roots` shape (`src/libgit2/transports/local.c:324-331`), and the depth-rejection shape in `negotiate_fetch` (`src/libgit2/transports/local.c:291-310`).
- `rhead->loid` is only consumed by push and by the smart and local transports internally, so the bundle transport does not need to populate it.
- `transport_find_fn` currently returns only `0` or `GIT_ENOTFOUND`, but `git_transport_new` propagates any other negative result (`src/libgit2/transport.c:80-137`), so bundle probing can and must preserve operational errors.
- `git_remote_stop` sets the transport cancellation flag, while disconnect closes but retains the transport object for reuse (`src/libgit2/remote.c:2179-2196`), so a bundle transport that consumes its cancellation flag must reset it for a new connection or negotiation.
- Source and test CMake files currently use globs, so new C files should not require CMake edits (`src/libgit2/CMakeLists.txt:23-25` covers `transports/*.c`; `tests/libgit2/CMakeLists.txt:27` uses `GLOB_RECURSE` over `tests/libgit2/*/*.c`, which covers `tests/libgit2/bundle/*.c`).
- `fuzzers/CMakeLists.txt:9` globs `*_fuzzer.c` and registers each target with `corpora/<name>` as its argument; the standalone driver fails if that directory is missing (`fuzzers/standalone_driver.c:54`).
- Fuzz targets link `${LIBGIT2_OBJECTS}` directly, so an internal-only parser entry point is reachable from a fuzz target without any public API.

Relevant Git behavior:

- The bundle-format documentation requires the prerequisite objects to be present, while the `git bundle verify` manual describes them as present and fully linked.
- Black-box testing against Git 2.50.1 shows that `git bundle verify` and bundle fetch check prerequisite-object existence, but do not reject a present prerequisite commit whose tree, blob, or parent closure is already corrupt.
- A normal `git bundle create <file> <branch>` records no `HEAD`; when cloning it, Git selects and checks out the configured initial branch only if that exact branch name is advertised.
- If the configured initial branch is not advertised, Git keeps it unborn and does not select the only branch or the first branch as a substitute.

Any of these facts should be rechecked if current `main` changes before implementation.

## Design

### Header parser and ownership

Add an internal bundle-header object containing:

- Bundle version.
- Object ID type.
- Prerequisite object IDs.
- Advertised `git_remote_head` entries.
- The byte offset of the first pack byte after a successful parse.

Keep parser and prerequisite-check code in `src/libgit2/bundle.c` and `src/libgit2/bundle.h`.
Keep transport state and callbacks in `src/libgit2/transports/bundle.c`.

Parse the header in these phases:

1. Require an exact v2 or v3 signature.
2. Read v3 capabilities.
3. Read prerequisites.
4. Read references.
5. Require the blank separator and stop at the first pack byte.

#### Source abstraction

The parser must not take a file descriptor.
Give it a minimal internal read abstraction: a small struct holding a read callback, an opaque payload, and the parser's own line buffer.
Provide two backings in the first pull request:

- A file-descriptor backing used by transport probing and connect.
- An in-memory backing over a `git_str` or a raw pointer and length.

Every parser test should be able to run against the in-memory backing, and only descriptor-positioning tests need the file backing.

This exists for two reasons.
It keeps parser tests free of temporary files, and it is the precondition for the deferred fuzz target.
Every existing libgit2 fuzz target consumes `LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)`, so a parser reachable only through a descriptor would force one temporary-file write per fuzz iteration.
The abstraction costs almost nothing now and is invasive to retrofit once the transport is built around a descriptor, so it belongs in the first commit even though the fuzz target itself is deferred.

Use incremental buffered line reads through that abstraction.
Do not read the whole header or pack into one buffer, and use overflow-checked allocation paths already established in libgit2.
Do not introduce an arbitrary whole-header size limit without an existing project convention or a demonstrated need.

Default v2 and v3 to SHA-1 unless v3 declares `object-format`.
Accept only object formats supported by the current build.
Reject capabilities in v2, malformed or conflicting capabilities, and unknown v3 capabilities.
Recognize `filter` but return `GIT_ENOTSUPPORTED`, because importing the pack without promisor metadata would be incorrect.

Preserve prerequisite comments only as ignorable syntax.
Own every advertised name and validate it with libgit2's reference-name rules before advertisement.

Locally detected failures must set a specific bundle error.
Errors propagated from lower layers should retain their original messages.
Any error intentionally ignored during transport probing must be cleared before selection continues.

Because the parser does not own a descriptor, the transport does.
The transport retains both the open descriptor and the parsed header through pack download so it does not reopen the path or rescan the header between connect and download.
After connect, the descriptor is left positioned at the pack offset the parser recorded.
Disconnect closes the descriptor and marks it closed, but keeps advertised refs available until the transport is freed or reconnected.
Reconnect frees the previous URL, descriptor, and header state before parsing again, and resets verification and cancellation state for the new connection.

### Transport detection

Do not add a URL scheme.
Select the bundle transport for a regular local file whose contents form a valid bundle header.

The local-path predicate must:

- Reject recognized scheme URLs, including `file://`.
- Reject scp-style paths whose colon precedes any path separator.
- Accept ordinary paths without a colon.
- Accept paths with a separator before a colon.
- Accept valid Windows drive-root paths.

Use existing path and URL helpers where possible.
Run bundle-file detection before the generic colon-based SSH fallback, while preserving custom-transport, scheme, and directory handling.
This prevents a Windows drive letter from being mistaken for an SSH host without changing genuine scp-style routing.

The insertion point is `transport_find_fn` in `src/libgit2/transport.c`, immediately before the colon-based SSH fallback at line 101.
A single insertion there is correct on both platforms.
Windows checks `git_fs_path_isdir` before the colon fallback and other systems check it after, but a bundle is a regular file, so both existing directory probes fall through to the new check either way.

The comment above the colon fallback currently says that other systems perform the SSH check first to avoid going to the filesystem when it is not necessary.
That statement stops being true once bundle detection runs first, so update it in the same commit rather than leaving a stale rationale for reviewers to catch.
The local-path predicate rejects scp-style `host:path` strings without touching the filesystem, so genuine SSH remotes still avoid the probe.

Give the probe an explicit outcome separate from its return code:

- Not a bundle.
- A supported bundle.
- A syntactically valid bundle that requires unsupported semantics.

Return zero when the probe produced one of those outcomes.
Propagate negative operational failures such as allocation, permission, and read errors through `transport_find_fn`; `git_transport_new` already preserves them.
Only an expected not-a-bundle or malformed-header result may clear its bundle-format error and continue through existing transport selection.

Classify the path before opening it.
The probe must first establish with a stat-family check that the path names an existing regular file; a missing path, a directory, or any other non-regular file is a not-a-bundle outcome that falls through silently, never an operational error.
This rule is load-bearing on non-Windows systems: the bundle probe runs before the colon fallback while the existing `git_fs_path_isdir` check runs after it, so a plain local repository path reaches the probe first, and `open` on a directory succeeds on POSIX while the subsequent read fails.
Treating that failure — or `ENOENT` on a nonexistent path — as a propagated operational error would break every ordinary local-directory clone and change the long-standing unsupported-URL error for bad paths.
Operational-error propagation applies only after the path is known to be an existing regular file.

Probe by parsing the complete header, not only its signature.
Close the probe descriptor in every case and parse again during connect, where the descriptor is retained as transport state.

The probe outcome should distinguish:

- Not a bundle or a malformed header: clear the probe error and continue through existing transport selection.
- A valid supported bundle: select the bundle transport.
- A syntactically valid bundle with unsupported semantics, including `filter` or an unknown v3 capability: select the bundle transport so connect can return the specific bundle error.

An unsupported capability must not stop the probe before syntax checking is complete.
Record the unsupported outcome, finish validating the header, and select the transport only if the rest of the header is syntactically valid.

This preserves normal local-repository errors for arbitrary or malformed files while producing useful errors for actual unsupported bundles.
Directories must continue to use the local repository transport.

### Reference advertisement and default branch

Expose every recorded reference through `ls`.
If the header records `HEAD`, move it to index zero with a stable in-place rotation so all other refs keep their order.
The bundle format records only an OID and a name, so every parsed entry, including `HEAD`, has no `symref_target`.

Some common bundles omit `HEAD`.
Do not synthesize one in the transport.
Git's bundle transport likewise leaves `HEAD` absent from `ls-remote`, and inventing one would make reference advertisement depend on destination configuration or an arbitrary branch-order choice.

Keep existing default-branch behavior for callers outside clone:

- If recorded `HEAD` is first, `git_remote__default_branch` scans advertised `refs/heads/*` entries whose OID matches it and prefers `git_repository_initialbranch` among the candidates.
- If `HEAD` is absent, `git_remote__default_branch` returns `GIT_ENOTFOUND`.

Teach clone's no-`HEAD` path to follow both libgit2 and Git precedent:

1. Read and validate the destination's configured initial `refs/heads/*` name with `git_repository_initialbranch`.
2. Scan the advertised heads for that exact full branch name.
3. If it is present, call clone's existing `update_head_to_branch` path with the short branch name.
   That path resolves the fetched remote-tracking ref, calls `update_head_to_new_branch`, configures the upstream, sets local `HEAD`, and permits the existing checkout path to run.
4. If the exact branch is absent, keep the current `update_head_to_default` behavior so the configured branch remains unborn and checkout is skipped.

Do not select the only branch, the first branch, a tag, or an OID match when the configured name is absent.
Do not create `refs/remotes/origin/HEAD`, because the remote did not advertise `HEAD`; `update_head_to_branch` already avoids doing so when `git_remote__default_branch` returns `GIT_ENOTFOUND`.
That avoidance is delicate rather than explicit: on `GIT_ENOTFOUND` the function zeroes the error and proceeds with an empty default-branch string, and only because an empty name matches no refspec does it skip `update_remote_head` (`src/libgit2/clone.c:281-290`), so the planned test asserting the absence of `refs/remotes/origin/HEAD` is what locks the behavior in.
Do not change the explicit `checkout_branch` path or the recorded-`HEAD` path.
A recorded `HEAD` whose OID matches no advertised branch continues to produce a detached clone through the existing logic.

This belongs in `src/libgit2/clone.c`, not the bundle transport, and its tests should use a custom transport that omits `HEAD` so the first commit is useful and green without bundle support.

Report the parsed object ID type through `oid_type`.
Report `GIT_REMOTE_CAPABILITY_TIP_OID`, because the transport can provide its advertised tips.
Do not report `GIT_REMOTE_CAPABILITY_REACHABLE_OID`, because a bundle cannot promise an arbitrary reachable object that was not packed.

### Prerequisite verification

Verify prerequisites by existence, matching Git 2.50.1's observed behavior for valid repositories and fetch-time unbundling.

When a bundle pack is needed, before creating an ODB write-pack:

- Require each prerequisite OID to exist in the destination object database.
- Read each prerequisite's object header so a missing or unreadable object fails with an error that identifies the prerequisite being checked.
- A bundle with no prerequisites succeeds against an empty destination.

Do not walk the prerequisite object graph.
Real bundles record rev-list boundary commits as prerequisites, and an existence check catches the case that matters — fetching an incremental bundle into a destination that lacks the base history.
This deliberately follows Git's observed implementation rather than claiming the stronger “fully linked” wording in the `git bundle verify` manual.
A destination whose ODB is already missing interior objects below an existing prerequisite is a corrupt repository, which neither Git nor this transport promises to detect at fetch time.

Thin-pack resolution is the backstop for delta bases: `git_odb_write_pack` resolves bases from the destination ODB, and a genuinely missing base fails the pack commit before any reference mutation.

#### No-pack fetches

The current fetch machinery skips `negotiate_fetch` and `download_pack` when every requested advertised tip OID already exists locally.
Do not distort advertisement or change the generic fetch path merely to force bundle verification in that case.
The first pull request checks prerequisites whenever it imports a bundle pack; a no-pack fetch may update refs to already-present OIDs without checking unused bundle prerequisites.
This is a narrow deviation from `git bundle verify`, but it does not introduce a new object dependency in a valid destination repository.
Document it in the pull request and lock it down with a test so the guarantee is not overstated later.

Perform the shallow-state checks, object-format comparison, and prerequisite existence check in `negotiate_fetch`, in that order, and mark the retained bundle state as verified only after all three succeed.
`download_pack` must require that verified state before creating an ODB write-pack.
Reset verification at the start of every negotiation and on connect, reconnect, and close so a result cannot leak across attempts, repositories, or bundle contents.

Reject a bundle whose object format differs from an existing destination repository before pack ingestion.
Clone into an empty repository continues to use the transport's `oid_type` callback to select the repository format.

### Pack ingestion

Stream bytes from the retained descriptor into `git_odb_write_pack` using the destination repository's ODB.
This lets the indexer resolve thin-pack bases from prerequisite objects.

Pass the configured transfer-progress callback and payload to the write-pack.
Check the transport cancellation flag between read chunks, while treating a nonzero callback result as the primary cancellation path.
Propagate read, append, callback, and commit errors.

Reset the cancellation flag when starting a new connection and at the beginning of each `negotiate_fetch` attempt.
Once negotiation begins, only `cancel` sets it until that attempt completes.
This makes `git_remote_stop` cancel the active operation without permanently poisoning a reusable transport.
This deliberately goes beyond existing precedent: neither the smart nor the local transport ever clears its cancellation flag, so both stay poisoned after a stop.
State in the pull request that the reset is an intentional robustness improvement, not an oversight, so the divergence from the mirrored transports reads as designed.

Commit the write-pack only after a clean EOF.
Free it on every path so a failed or cancelled download cannot publish an incomplete pack.
Reference updates remain the existing fetch machinery's responsibility and must not occur after a download failure.

A bundle pack cannot be sliced by refspec.
When the fetch machinery requests a download, ingest the whole pack even if only some advertised refs are wanted.
Tests should verify selective ref updates without claiming selective object transfer.

### Transport callback surface

Populate every callback in `git_transport` that the fetch machinery invokes unconditionally.
The fetch path does not check these for NULL, so an omitted callback is a crash rather than a degraded feature.

Two are easy to overlook:

- `negotiate_fetch` is called unconditionally from `git_fetch_negotiate` whenever a pack is needed.
  The bundle transport uses it to reject a nonzero `depth` or any existing destination shallow roots with `GIT_ENOTSUPPORTED` before verification or ingestion.
  Rejecting `wants->shallow_roots_len > 0` is required because returning an empty remote shallow-root array after download would otherwise remove the destination's `shallow` file.
  It does not need to populate `rhead->loid`, which only push and the smart and local transports consume.
- `shallow_roots` is called unconditionally from `git_fetch_download_pack` immediately after `download_pack` returns.
  After negotiation has established that the destination is not shallow, a bundle has no shallow roots, so implement it as a no-op: leave the output array empty and return zero.
  Leaving this NULL segfaults on every successful bundle fetch, which is why it is called out here rather than left implicit.

Also provide `set_connect_opts`, `capabilities`, `oid_type`, `ls`, `push`, `is_connected`, `cancel`, `close`, and `free`.
Mirror the local transport for the connection-state callbacks so disconnect and reconnect behave the way the rest of the library expects.

### Unsupported operations

Return `GIT_ENOTSUPPORTED` for push both at connection time and through the push callback.
Reject any non-full depth, including an unshallow request, or existing destination shallow roots in `negotiate_fetch`, before prerequisite verification or pack ingestion.
Reject filtered bundles as described by the parser rules.

## Expected files

Implementation:

- `src/libgit2/clone.c`
- `src/libgit2/bundle.c`
- `src/libgit2/bundle.h`
- `src/libgit2/transports/bundle.c`
- `src/libgit2/transport.c`
- `include/git2/sys/transport.h`

Tests and fixtures:

- Focused generic no-`HEAD` clone tests under the existing clone transport tests.
- `tests/libgit2/bundle/parse.c`
- `tests/libgit2/clone/bundle.c`
- `tests/libgit2/fetch/bundle.c`
- Focused transport-selection and advertisement tests under the existing network-remote tests.
- Static files and a generation script under `tests/resources/bundle/`.

Prerequisite checks are exercised through the fetch tests rather than a dedicated verification test file.

Documentation:

- `docs/changelog.md`

No CMake edit is expected, but confirm that assumption when adding the parser and transport sources.
Do not add a differences document unless the final tested behavior has a durable user-visible divergence worth documenting.

## Fixture strategy

Use committed static fixtures so tests do not require an external Git executable.
Prefer histories already represented by `testrepo.git` and `testrepo_256.git` when that keeps expected OIDs familiar.

Commit a reproducible generation script beside the fixtures.
The script must disable configured commit signing with `commit.gpgsign=false` and pin any other inputs that affect object IDs.

Minimum fixtures:

- A self-contained SHA-1 v2 bundle created with `--all`, with recorded `HEAD` after other refs.
- A self-contained SHA-1 v2 bundle with one branch and no recorded `HEAD`.
- A self-contained SHA-1 v2 bundle whose recorded `HEAD` OID matches no advertised branch.
- A self-contained SHA-256 v3 bundle with `object-format=sha256`.
- An incremental SHA-1 bundle with a prerequisite and a thin pack.

Construct small malformed headers directly in tests when a binary fixture would obscure the condition being tested.

## Test matrix

### Parser

- Parse valid v2 SHA-1, v3 SHA-1, and v3 SHA-256 headers.
- Preserve prerequisite OIDs while ignoring their comments.
- Preserve ref OIDs, names, and input order.
- Record the offset of the first `PACK` byte, and confirm the descriptor sits there after a file-backed parse.
- Run the shared header cases through both the in-memory and file-descriptor backings and assert identical results, which keeps the fuzzable entry point exercised by ordinary CI.
- Reject invalid or truncated signatures and a missing header separator.
- Reject malformed object IDs, missing ref names, invalid ref names, embedded NUL, and embedded carriage returns.
- Reject capabilities in v2, malformed or conflicting capabilities, unknown v3 capabilities, and `filter` with the correct error class.
- Exercise long and chunk-boundary-spanning lines without whole-file buffering.
- Assert specific locally generated errors and preserved lower-level errors.

### Prerequisites

- Pass a self-contained bundle against an empty repository.
- Fetch an incremental thin bundle when its prerequisites exist.
- Pass prerequisites satisfied by objects not reachable from any persistent ref.
- Fail before pack or ref mutation when a prerequisite is missing, with an error identifying the prerequisite.
- Fail an object-format mismatch before pack or ref mutation.

### Detection and advertisement

- Select a bundle by content at extensionless and arbitrary-extension paths.
- Leave directories on the local transport.
- Preserve the existing unsupported-URL error for a nonexistent local path.
- Do not classify an arbitrary or malformed regular file as a bundle.
- Select a syntactically valid but unsupported bundle and report its specific connect error.
- Treat an unknown v3 capability as a syntactically valid unsupported bundle, rather than falling through to another transport.
- Propagate an injected allocation or read failure from probing instead of clearing it as a format miss.
- Distinguish local paths containing colons from scp-style `host:path` strings.
- Cover Windows drive-letter selection in Windows CI.
- Advertise all recorded refs and move recorded `HEAD` to the first position.
- Confirm every parsed entry, including a recorded `HEAD`, has no symref target because the bundle format carries none.
- Do not synthesize `HEAD` for a bundle that omits it.
- Confirm the existing default-branch logic prefers the configured initial branch when several advertised branches share the recorded `HEAD` OID, without the transport consulting the repository.
- Confirm `git_remote__default_branch` returns `GIT_ENOTFOUND` when `HEAD` is absent.
- Keep advertised refs available after disconnect.
- Report `TIP_OID` but not `REACHABLE_OID`.
- Reject push and nonzero-depth requests, the latter through `negotiate_fetch`.
- Reject a bundle download into an already-shallow destination before pack or ref mutation and preserve the existing `shallow` file byte-for-byte.
- Return an empty shallow-root array from `shallow_roots` after a successful negotiation against a non-shallow destination.

### Clone

- Through a custom transport that omits `HEAD`, select the configured initial branch when that exact advertised branch was fetched; verify the local branch, upstream, checkout, and absence of `refs/remotes/origin/HEAD`.
- Through the same generic test transport, keep the configured initial branch unborn when its name is absent, even when exactly one other branch is advertised.
- Cover several advertised branches and prove that exact-name selection is independent of advertisement order.
- Preserve existing empty-remote, explicit `checkout_branch`, recorded-`HEAD`, detached-`HEAD`, bare-clone, and tracking behavior.
- Clone a self-contained v2 SHA-1 bundle and verify `HEAD`, checkout, remote-tracking refs, and representative objects.
- Clone the single-branch bundle without recorded `HEAD` when its branch matches the configured initial branch, and verify the local branch, upstream, checkout, and absence of a synthesized remote `HEAD`.
- Clone the same bundle with a different configured initial branch, and verify that the configured branch remains unborn and no arbitrary bundle branch is selected, matching Git.
- Clone a bundle whose recorded `HEAD` OID matches no advertised branch and verify the existing detached-HEAD behavior.
- Clone a v3 SHA-256 bundle and verify the destination object format.
- Fail cleanly when cloning an incremental bundle into an empty repository.

### Fetch

- Fetch a self-contained bundle into an existing repository and update the expected remote-tracking refs.
- Fetch an incremental thin bundle when its prerequisites exist.
- Fail before object or ref mutation when a needed-pack prerequisite is missing.
- When every requested tip OID already exists locally, skip pack ingestion and allow ref updates without checking unused bundle prerequisites; assert that no pack or new object is written.
- Reject a bundle whose object format differs from the destination repository.
- Surface truncated and corrupt pack errors without updating refs.
- Update only refs selected by refspec while tolerating unrelated objects in the imported pack.
- Invoke transfer-progress callbacks.
- Propagate callback cancellation without updating refs.
- After cancellation through `git_remote_stop`, retry with the same `git_remote` and verify that reconnect or the new negotiation resets cancellation and succeeds.

## Commit structure

Use one pull request unless maintainers ask for a split.
The parser is internal support for one transport, so a separate pull request would create temporarily unused internal code and more review coordination without a stable API boundary.

Use three functional commits:

1. `clone: select the initial branch when HEAD is absent`
   Add the generic clone-policy fallback in `src/libgit2/clone.c` and custom-transport tests that omit `HEAD`.
   Reuse `update_head_to_branch` and the existing local-branch, upstream, and checkout flow; do not add bundle-specific policy.
2. `bundle: parse bundle headers`
   Add the internal representation, the read abstraction with its file and memory backings, the parser, fixtures needed by parser tests, and focused tests.
3. `bundle: fetch and clone from bundle files`
   Add detection and its transport-comment update, probe outcome classification with operational-error propagation, the full transport callback surface including `negotiate_fetch` and `shallow_roots`, recorded-HEAD ordering, prerequisite existence checks, object-format checks, cancellation reset, pack ingestion, integration tests, remaining fixtures, and the changelog entry.

Every commit must build and pass its focused tests.
Do not create a mechanical intermediate commit that preserves known leaks, stale error handling, missing verification, or other broken behavior from PR #7101.

The relevant commit bodies and pull request description should say that the implementation is based on PR #7101 by Laurence McGlashan.
Do not use `--author` or a `Co-authored-by:` trailer without the contributor's agreement.

Open a fresh pull request rather than rewriting PR #7221.
Closing #7221 as superseded is a separate external action that requires approval when the new pull request exists.

## Pull request description

The description should:

- State that the scope is local bundle clone and fetch plus the narrow generic no-`HEAD` clone fallback needed to make ordinary bundle clones match Git.
- Credit Laurence McGlashan and PR #7101.
- Explain that the history is organized as functional commits rather than preserving a broken baseline snapshot.
- Call out the two incorporated #7221 review points: no invented protocol and `HEAD`-first advertisement.
- Describe content-based local-path detection and its SSH and Windows guards.
- Explain that the transport does not synthesize a missing `HEAD`, that bundle references contain no symref metadata, and that a recorded `HEAD` is resolved by its OID through existing default-branch logic.
- Explain the generic clone fallback: exact configured initial-branch name only, reuse of fetched remote-tracking and local-branch helpers, no arbitrary branch selection, and no synthesized `refs/remotes/origin/HEAD`.
- Explain that prerequisite verification follows Git 2.50.1's observed existence check for valid repositories, while the Git manual uses stronger “fully linked” wording; thin-pack resolution remains the backstop for genuinely missing bases.
- Disclose that no-pack fetches may update refs to already-present OIDs without checking unused prerequisites because the generic fetch path skips transport negotiation when no pack is needed.
- Call out rejection of both requested depth and an already-shallow destination before bundle pack ingestion.
- Note that the header parser is driven through a read abstraction so it can be fuzzed from memory, and that a fuzz target is a planned follow-up rather than an omission.
- State that expected format misses, nonexistent paths, and non-regular files fall through transport probing, but allocation, permission, and read errors on an existing regular file propagate.
- State that cancellation is reset for a new connection or negotiation, that retry after cancellation is covered, and that this reset intentionally goes beyond the smart and local transports, which never clear their cancellation flags.
- State that the transport reports advertised-tip but not arbitrary-reachable-OID capability.
- List SHA-1, SHA-256, clone, fetch, prerequisite, progress, cancellation, and failure-path coverage.
- Reference #1718, #6824, #7101, and #7221 without closing the bundle-creation requests.

## Verification

Before opening the pull request:

1. Configure and build with tests enabled.
2. Run each new focused test group.
3. Run the full `libgit2_tests` suite or `ctest --output-on-failure`.
4. Regenerate fixtures and confirm the Git CLI can list and verify the valid fixtures.
5. Confirm invalid fixtures exercise the intended failure rather than incidental corruption.
6. Run `git diff --check` and inspect the diff against current `upstream/main` for unrelated changes.
7. Verify each commit independently builds and passes the tests introduced by that commit.
8. Confirm Windows CI covers drive-letter detection and ordinary CI covers both object formats.

No implementation files should be edited until this plan is accepted.

## Implementation outcome

The parser and transport were implemented on `git-bundles-clone-fetch`.
The initial implementation also included the generic no-`HEAD` clone-policy commit described above, but that change was backed out after review to preserve existing libgit2 behavior.
All eight verification steps were run except where noted under unverified coverage.
The functional bundle changes configure, build, and pass their focused tests; `git diff --check` is clean; the fixtures regenerate byte for byte and Git 2.50.1 accepts all five.
No CMake edits were required, confirming the assumption recorded under sources and constraints.
`git_transport_bundle` is exposed from `include/git2/sys/transport.h`, taking the option this plan left open.

### Deviations from this plan

An unsupported `object-format` returns `GIT_ENOTSUPPORTED` immediately rather than finishing syntax checking first.
That capability fixes the width of every remaining object id, so no meaningful syntax remains to check, and parsing on would report a real bundle in an unknown format as malformed instead of unsupported.
`filter` and unknown capabilities still record the outcome and continue, as this plan requires.
The reasoning is in a comment at `src/libgit2/bundle.c:242`.

No bundle-specific error class was added.
This plan asked for a specific bundle error but did not list `include/git2/errors.h` among the expected files, and adding a value to a public enum is an API addition worth its own decision.
Locally detected failures use `GIT_ERROR_INVALID` for header and format problems, `GIT_ERROR_OS` for descriptor failures, `GIT_ERROR_ODB` for a missing prerequisite, and `GIT_ERROR_NET` for transport-level refusals.
A `GIT_ERROR_BUNDLE` class remains available if maintainers want one.

`download_pack` seeks to the recorded pack offset before reading, rather than relying on the position connect left behind.
Without it, a retry after a cancelled or failed download would resume in the middle of the pack.
This makes `download_pack` idempotent for the cost of one `lseek`.

The fixed bundle signature line has an exact length bound, while subsequent valid header lines remain unbounded.
This prevents content-based probing from buffering an arbitrarily large first line from a non-bundle file without imposing a format limit on reference names.

Path probing retains existing transport-selection behavior when its initial stat cannot classify a path.
Once a path is known to be an existing regular file, allocation, open, and read failures propagate as operating-system errors.

### Behaviour change avoided after review

The planned generic no-`HEAD` fallback made cloning a repository whose own `HEAD` is unborn check out libgit2's configured initial branch instead of preserving the existing unborn result.
Clone cannot currently distinguish that case from a bundle that records no `HEAD`, because libgit2's transports do not carry an unborn `HEAD`'s symref target.
Rather than trade one behavior for another or make generic clone policy bundle-aware, the fallback was removed.
A bundle without `HEAD` therefore preserves existing libgit2 behavior: it fetches the advertised refs and objects, leaves the configured initial branch unborn, and performs no checkout.

### Coverage not achieved

These test-matrix entries are not covered by the committed tests:

- Prerequisites satisfied by objects not reachable from any persistent ref.
- An injected allocation failure during probing.
  Propagation is covered by a permission-denied regular file, which exercises the same path for a read failure.
- A corrupt pack distinct from a truncated one.
  Truncation is covered.
- A direct assertion that `shallow_roots` returns an empty array.
  This is covered indirectly by asserting that a successful fetch leaves no `shallow` file.
- Windows drive-letter detection on Windows CI.
  A test connects through the absolute fixture path, which is drive-rooted on Windows, but it has not been run there.

## Follow-ups

### Unborn remote HEAD

Teach the transports to advertise an unborn `HEAD` with its symref target, the way `upload-pack` does, so clone can tell a remote whose `HEAD` is unborn from one that records no `HEAD`.
That would let a future change honour the remote's target in the first case and apply the configured initial branch only in the second, matching Git for both without regressing existing clone behavior.
This touches `src/libgit2/transports/local.c`, the smart transport's symref handling, and clone's recorded-`HEAD` path, so it remains outside this bundle transport change.

### Clar sandbox cleanup

`test_clone_nonetwork__clone_tag_to_tree` frees the clar sandbox repository directly and calls `cl_fixture_cleanup` without clearing clar's statics, leaving `_cl_repo` dangling.
Any later suite that calls `cl_git_sandbox_cleanup` without first calling `cl_git_sandbox_init` then double-frees it and crashes the run.
This is pre-existing and unrelated to bundles; the new suites work around it by cleaning the sandbox only when they created one.
The fix belongs in that test, not in the workarounds.


### Bundle creation

Bundle creation is a separate change after the read transport is reviewed or merged.
That work can evaluate a public bundle API with maintainer input.

### Fuzz target

Add a header-parser fuzz target as a follow-up after the parser shape is stable.
Keeping it out of the first pull request reduces scope without weakening the deterministic parser and integration coverage required here, and the read abstraction added in commit 2 means the follow-up needs no parser changes.

The repository already has the infrastructure, so the follow-up should follow existing convention rather than invent one:

- Add `fuzzers/bundle_fuzzer.c` with the standard `LLVMFuzzerInitialize` and `LLVMFuzzerTestOneInput` pair, driving the parser through its in-memory backing.
- No CMake edit is needed, because `fuzzers/CMakeLists.txt` globs `*_fuzzer.c`.
- Add `fuzzers/corpora/bundle/` with seed inputs.
  This directory is mandatory: each target is registered with `corpora/<name>` as its argument, and the standalone driver fails outright if the directory is missing, which would break CI.
- Fuzz targets link `${LIBGIT2_OBJECTS}`, so the internal parser entry point is reachable without exposing any public API.
- `f015996fe` is a good template commit for adding a single new target.

Both CI workflows already build with `BUILD_FUZZERS=ON` and `USE_STANDALONE_FUZZERS=ON` and replay the corpora through `ctest -R fuzzer`, so a new target and its corpus become a regression test on every pull request with no workflow changes.
Note that `BUILD_FUZZERS` is incompatible with `BUILD_TESTS`, which is why fuzzing runs as its own job.

A transport-level fuzz target over whole bundle files is a possible second step.
`fuzzers/download_refs_fuzzer.c` is the closest existing model: it feeds fuzz bytes through a mock subtransport into a full `git_remote_connect` and `git_remote_download` against a scratch repository from `fuzzer_repo_init()`.
