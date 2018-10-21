#!/usr/bin/env bash

set -e

if [ -n "$SKIP_TESTS" ]; then
	exit 0
fi

SOURCE_DIR=${SOURCE_DIR:-$( cd "$( dirname "${BASH_SOURCE[0]}" )" && dirname $( pwd ) )}
BUILD_DIR=$(pwd)
TMPDIR=${TMPDIR:-/tmp}
USER=${USER:-$(whoami)}

SUCCESS=1

VALGRIND="valgrind --leak-check=full --show-reachable=yes --error-exitcode=125 --num-callers=50 --suppressions=\"$SOURCE_DIR/libgit2_clar.supp\""
LEAKS="MallocStackLogging=1 MallocScribble=1 MallocLogFile=/dev/null CLAR_AT_EXIT=\"leaks -quiet \$PPID\""

cleanup() {
	echo "Cleaning up..."

	if [ ! -z "$GITDAEMON_DIR" -a -f "${GITDAEMON_DIR}/pid" ]; then
		echo "Stopping git daemon..."
		kill $(cat "${GITDAEMON_DIR}/pid")
	fi

	if [ ! -z "$SSHD_DIR" -a -f "${SSHD_DIR}/pid" ]; then
		echo "Stopping SSH..."
		kill $(cat "${SSHD_DIR}/pid")
	fi

	echo "Done."
}

failure() {
	echo "Test exited with code: $1"
	SUCCESS=0
}

# Ask ctest what it would run if we were to invoke it directly.  This lets
# us manage the test configuration in a single place (tests/CMakeLists.txt)
# instead of running clar here as well.  But it allows us to wrap our test
# harness with a leak checker like valgrind.  Append the option to write
# JUnit-style XML files.
run_test() {
	TEST_CMD=$(ctest -N -V -R "^${1}$" | sed -n 's/^[0-9]*: Test command: //p')

	if [ -z "$TEST_CMD" ]; then
		echo "Could not find tests: $1"
		exit 1
	fi

	TEST_CMD="${TEST_CMD} -r${BUILD_DIR}/results_${1}.xml"

	if [ "$LEAK_CHECK" = "valgrind" ]; then
		RUNNER="$VALGRIND $TEST_CMD"
	elif [ "$LEAK_CHECK" = "leaks" ]; then
		RUNNER="$LEAKS $TEST_CMD"
	else
		RUNNER="$TEST_CMD"
	fi

	eval $RUNNER || failure
}

# Configure the test environment; run them early so that we're certain
# that they're started by the time we need them.

echo "##############################################################################"
echo "## Configuring test environment"
echo "##############################################################################"

if [ -z "$SKIP_GITDAEMON_TESTS" ]; then
	echo "Starting git daemon..."
	GITDAEMON_DIR=`mktemp -d ${TMPDIR}/gitdaemon.XXXXXXXX`
	git init --bare "${GITDAEMON_DIR}/test.git"
	git daemon --listen=localhost --export-all --enable=receive-pack --pid-file="${GITDAEMON_DIR}/pid" --base-path="${GITDAEMON_DIR}" "${GITDAEMON_DIR}" 2>/dev/null &
fi

if [ -z "$SKIP_PROXY_TESTS" ]; then
	echo "Starting HTTP proxy..."
	curl -L https://github.com/ethomson/poxyproxy/releases/download/v0.2.0/poxyproxy-0.2.0.jar >poxyproxy.jar
	java -jar poxyproxy.jar -d --address 127.0.0.1 --port 8080 --credentials foo:bar >/dev/null 2>&1 &
fi

if [ -z "$SKIP_SSH_TESTS" ]; then
	echo "Starting ssh daemon..."
	HOME=`mktemp -d ${TMPDIR}/home.XXXXXXXX`
	SSHD_DIR=`mktemp -d ${TMPDIR}/sshd.XXXXXXXX`
	git init --bare "${SSHD_DIR}/test.git"
	cat >"${SSHD_DIR}/sshd_config" <<-EOF
	Port 2222
	ListenAddress 0.0.0.0
	Protocol 2
	HostKey ${SSHD_DIR}/id_rsa
	PidFile ${SSHD_DIR}/pid
	AuthorizedKeysFile ${HOME}/.ssh/authorized_keys
	LogLevel DEBUG
	RSAAuthentication yes
	PasswordAuthentication yes
	PubkeyAuthentication yes
	ChallengeResponseAuthentication no
	StrictModes no
	# Required here as sshd will simply close connection otherwise
	UsePAM no
	EOF
	ssh-keygen -t rsa -f "${SSHD_DIR}/id_rsa" -N "" -q
	/usr/sbin/sshd -f "${SSHD_DIR}/sshd_config" -E "${SSHD_DIR}/log"

	# Set up keys
	mkdir "${HOME}/.ssh"
	ssh-keygen -t rsa -f "${HOME}/.ssh/id_rsa" -N "" -q
	cat "${HOME}/.ssh/id_rsa.pub" >>"${HOME}/.ssh/authorized_keys"
	while read algorithm key comment; do
		echo "[localhost]:2222 $algorithm $key" >>"${HOME}/.ssh/known_hosts"
	done <"${SSHD_DIR}/id_rsa.pub"

	# Get the fingerprint for localhost and remove the colons so we can
	# parse it as a hex number. Older versions have a different output
	# format.
	if [[ $(ssh -V 2>&1) == OpenSSH_6* ]]; then
		SSH_FINGERPRINT=$(ssh-keygen -F '[localhost]:2222' -f "${HOME}/.ssh/known_hosts" -l | tail -n 1 | cut -d ' ' -f 2 | tr -d ':')
	else
		SSH_FINGERPRINT=$(ssh-keygen -E md5 -F '[localhost]:2222' -f "${HOME}/.ssh/known_hosts" -l | tail -n 1 | cut -d ' ' -f 3 | cut -d : -f2- | tr -d :)
	fi
fi

# Run the tests that do not require network connectivity.

if [ -z "$SKIP_OFFLINE_TESTS" ]; then
	echo ""
	echo "##############################################################################"
	echo "## Running (offline) tests"
	echo "##############################################################################"

	run_test offline
fi

if [ -z "$SKIP_ONLINE_TESTS" ]; then
	# Run the various online tests.  The "online" test suite only includes the
	# default online tests that do not require additional configuration.  The
	# "proxy" and "ssh" test suites require further setup.

	echo ""
	echo "##############################################################################"
	echo "## Running (online) tests"
	echo "##############################################################################"

	run_test online
fi

if [ -z "$SKIP_GITDAEMON_TESTS" ]; then
	echo ""
	echo "Running gitdaemon tests"
	echo ""

	export GITTEST_REMOTE_URL="git://localhost/test.git"
	run_test gitdaemon
	unset GITTEST_REMOTE_URL
fi

if [ -z "$SKIP_PROXY_TESTS" ]; then
	echo ""
	echo "Running proxy tests"
	echo ""

	export GITTEST_REMOTE_PROXY_HOST="localhost:8080"
	export GITTEST_REMOTE_PROXY_USER="foo"
	export GITTEST_REMOTE_PROXY_PASS="bar"
	run_test proxy
	unset GITTEST_REMOTE_PROXY_HOST
	unset GITTEST_REMOTE_PROXY_USER
	unset GITTEST_REMOTE_PROXY_PASS
fi

if [ -z "$SKIP_SSH_TESTS" ]; then
	echo ""
	echo "Running ssh tests"
	echo ""

	export GITTEST_REMOTE_URL="ssh://localhost:2222/$SSHD_DIR/test.git"
	export GITTEST_REMOTE_USER=$USER
	export GITTEST_REMOTE_SSH_KEY="${HOME}/.ssh/id_rsa"
	export GITTEST_REMOTE_SSH_PUBKEY="${HOME}/.ssh/id_rsa.pub"
	export GITTEST_REMOTE_SSH_PASSPHRASE=""
	export GITTEST_REMOTE_SSH_FINGERPRINT="${SSH_FINGERPRINT}"
	run_test ssh
	unset GITTEST_REMOTE_URL
	unset GITTEST_REMOTE_USER
	unset GITTEST_REMOTE_SSH_KEY
	unset GITTEST_REMOTE_SSH_PUBKEY
	unset GITTEST_REMOTE_SSH_PASSPHRASE
	unset GITTEST_REMOTE_SSH_FINGERPRINT
fi

if [ -z "$SKIP_FUZZERS" ]; then
	echo ""
	echo "##############################################################################"
	echo "## Running fuzzers"
	echo "##############################################################################"

	for fuzzer in fuzzers/*_fuzzer; do
		"${fuzzer}" "${SOURCE_DIR}/fuzzers/corpora/$(basename "${fuzzer%_fuzzer}")" || failure
	done
fi

cleanup

if [ "$SUCCESS" -ne "1" ]; then
	echo "Some tests failed."
	exit 1
fi

echo "Success."
exit 0
