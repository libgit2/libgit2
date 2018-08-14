#!/bin/bash

set -e

# Environment check
[ -z "$COVERITY_TOKEN" ] && echo "Need to set a coverity token" && exit 1

SOURCE_DIR=${SOURCE_DIR:-$( cd "$( dirname "${BASH_SOURCE[0]}" )" && dirname $( pwd ) )}
BUILD_DIR=$(pwd)

case $(uname -m) in
	i?86)				BITS=32 ;;
	amd64|x86_64)	BITS=64 ;;
esac
SCAN_TOOL=https://scan.coverity.com/download/cxx/linux${BITS}
TOOL_BASE=$(pwd)/_coverity-scan

# Install coverity tools
if [ ! -d "$TOOL_BASE" ]; then
	echo "Downloading coverity..."
	mkdir -p "$TOOL_BASE"
	pushd "$TOOL_BASE"
	wget -O coverity_tool.tgz $SCAN_TOOL \
		--post-data "project=libgit2&token=$COVERITY_TOKEN"
	tar xzf coverity_tool.tgz
	popd
	TOOL_DIR=$(find "$TOOL_BASE" -type d -name 'cov-analysis*')
	ln -s "$TOOL_DIR" "$TOOL_BASE"/cov-analysis
fi

cp "${SOURCE_DIR}/script/user_nodefs.h" "$TOOL_BASE"/cov-analysis/config/user_nodefs.h

COV_BUILD="$TOOL_BASE/cov-analysis/bin/cov-build"

# Configure and build
cmake ${SOURCE_DIR}

COVERITY_UNSUPPORTED=1 \
	$COV_BUILD --dir cov-int \
	cmake --build .

# Upload results
tar czf libgit2.tgz cov-int
SHA=$(cd ${SOURCE_DIR} && git rev-parse --short HEAD)

HTML="$(curl \
	--silent \
	--write-out "\n%{http_code}" \
	--form token="$COVERITY_TOKEN" \
	--form email=libgit2@gmail.com \
	--form file=@libgit2.tgz \
	--form version="$SHA" \
	--form description="libgit2 build" \
	https://scan.coverity.com/builds?project=libgit2)"
# Body is everything up to the last line
BODY="$(echo "$HTML" | head -n-1)"
# Status code is the last line
STATUS_CODE="$(echo "$HTML" | tail -n1)"

echo "${BODY}"

if [ "${STATUS_CODE}" != "200" -a "${STATUS_CODE}" != "201" ]; then
	echo "Received error code ${STATUS_CODE} from Coverity"
	exit 1
fi
