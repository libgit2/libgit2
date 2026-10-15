#!/usr/bin/env bash
#
# Regenerate the bundle fixtures from the testrepo fixtures.
#
# Every input that affects an object id is pinned here, so re-running
# this script must reproduce the committed files byte for byte.
#
# To update the fixtures, from the test resource directory:
#     sh ./bundle/generate.sh

set -e

cd "$(dirname "$0")"

resources="$(cd .. && pwd)"
out="$(pwd)"
tmp="$(mktemp -d)"

trap 'rm -rf "$tmp"' EXIT

git_pinned() {
	git \
		-c commit.gpgsign=false \
		-c tag.gpgsign=false \
		-c gc.auto=0 \
		-c pack.threads=1 \
		-c user.name="Bundle Fixture" \
		-c user.email="bundle@example.com" \
		"$@"
}

# A self-contained SHA-1 v2 bundle of every reference, with HEAD
# recorded after the other references.
git_pinned -C "$resources/testrepo.git" bundle create "$out/testrepo.bundle" --all HEAD

# A self-contained SHA-1 v2 bundle of a single branch and no HEAD.
git_pinned -C "$resources/testrepo.git" bundle create "$out/nohead.bundle" refs/heads/master

# A self-contained SHA-1 v2 bundle whose recorded HEAD matches no
# advertised branch.
git_pinned -C "$resources/testrepo.git" bundle create "$out/detached.bundle" HEAD refs/heads/br2

# A self-contained SHA-256 v3 bundle.
git_pinned -C "$resources/testrepo_256.git" bundle create "$out/testrepo_256.bundle" --all HEAD

# An incremental SHA-1 bundle: one new commit on top of testrepo.git's
# master, packed thin against master as a prerequisite.
cp -R "$resources/testrepo.git" "$tmp/incremental.git"
rm -f "$tmp/incremental.git/index"

export GIT_AUTHOR_NAME="Bundle Fixture"
export GIT_AUTHOR_EMAIL="bundle@example.com"
export GIT_AUTHOR_DATE="@1136214245 +0000"
export GIT_COMMITTER_NAME="Bundle Fixture"
export GIT_COMMITTER_EMAIL="bundle@example.com"
export GIT_COMMITTER_DATE="@1136214245 +0000"

base=$(git_pinned -C "$tmp/incremental.git" rev-parse refs/heads/master)
tree=$(git_pinned -C "$tmp/incremental.git" rev-parse "$base^{tree}")
new=$(git_pinned -C "$tmp/incremental.git" commit-tree -p "$base" -m "incremental bundle fixture" "$tree")

git_pinned -C "$tmp/incremental.git" update-ref refs/heads/master "$new"
git_pinned -C "$tmp/incremental.git" bundle create "$out/incremental.bundle" "$base..refs/heads/master"

echo "wrote:"
ls -l "$out"/*.bundle
