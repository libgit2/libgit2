#!/usr/bin/env bash
#
# Generate bundle test fixtures.
#
# Run from the tests/resources/bundle/ directory:
#
#   cd tests/resources/bundle && bash generate.sh
#
# Requires git >= 2.0 with bundle support.
# The generated files are committed to the repository.
#
set -e -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TESTREPO="$(cd "$SCRIPT_DIR/../testrepo.git" && pwd)"

cd "$SCRIPT_DIR"

# -------------------------------------------------------------------------
# Full bundle: all of testrepo (no prerequisites)
# -------------------------------------------------------------------------
git -C "$TESTREPO" bundle create "$SCRIPT_DIR/testrepo.bundle" --all
echo "Created testrepo.bundle"

# -------------------------------------------------------------------------
# Incremental bundle: only commits newer than a4a7dce (first commit tip)
# Uses a well-known old commit as the prerequisite.
# The prerequisite is the first commit on br2 so almost every other branch
# will have it as an ancestor.
# -------------------------------------------------------------------------
# We pick "refs/heads/br2" to include and hide the first commit of master
# so that the prerequisite is well-defined.
PREREQ=$(git -C "$TESTREPO" rev-list --max-parents=0 refs/heads/master)
git -C "$TESTREPO" bundle create "$SCRIPT_DIR/testrepo_prereq.bundle" \
    refs/heads/master refs/heads/br2 \
    "^$PREREQ"
echo "Created testrepo_prereq.bundle (prerequisite: $PREREQ)"

# -------------------------------------------------------------------------
# Text fixtures for header parsing tests (no packfile — contains only the
# header up to and including the separator).
# These are constructed by hand; they carry an empty PACK file (4 bytes
# "PACK" + 4 bytes version + 4 bytes count + 20 bytes SHA1 trailer = 32 bytes
# for an empty pack).
# -------------------------------------------------------------------------

# Empty pack bytes (valid PACK\0\0\0\2\0\0\0\0 + SHA1 of that)
EMPTY_PACK=$(printf 'PACK\x00\x00\x00\x02\x00\x00\x00\x00')
EMPTY_PACK_SHA=$(printf 'PACK\x00\x00\x00\x02\x00\x00\x00\x00' | openssl sha1 -binary)

write_minimal_bundle() {
    local outfile="$1"
    # remaining args are header lines (each without trailing newline)
    shift
    local line
    # write header lines
    for line in "$@"; do
        printf '%s\n' "$line" >> "$outfile"
    done
    # write blank separator line
    printf '\n' >> "$outfile"
    # append empty packfile
    printf 'PACK\x00\x00\x00\x02\x00\x00\x00\x00' >> "$outfile"
    printf '%s' "$EMPTY_PACK_SHA" >> "$outfile"
}

# Fake OIDs (40 hex zeros = a valid-looking but non-existent SHA1)
ZERO_OID="0000000000000000000000000000000000000000"
ONE_OID="1111111111111111111111111111111111111111"

# v2 bundle with one ref (no prerequisites)
rm -f v2.bundle
write_minimal_bundle v2.bundle \
    "# v2 git bundle" \
    "$ZERO_OID refs/heads/main"
echo "Created v2.bundle"

# v3 bundle with sha1 (no prerequisites)
rm -f v3.bundle
write_minimal_bundle v3.bundle \
    "# v3 git bundle" \
    "@object-format=sha1" \
    "$ZERO_OID refs/heads/main"
echo "Created v3.bundle"

# v2 bundle with prerequisite
rm -f v2_prereq.bundle
write_minimal_bundle v2_prereq.bundle \
    "# v2 git bundle" \
    "-${ONE_OID} prerequisite" \
    "${ZERO_OID} refs/heads/main"
echo "Created v2_prereq.bundle"

echo "Done."
