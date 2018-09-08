#!/usr/bin/env bash

set -e

if [ -n "$SKIP_TESTS" ]; then
	exit 0
fi

cleanup() {
	echo "Cleaning up..."

	if [ ! -z "$GITDAEMON_DIR" -a -f "${GITDAEMON_DIR}/pid" ]; then
		kill $(cat "${GITDAEMON_DIR}/pid")
	fi

	if [ ! -z "$SSHD_DIR" -a -f "${SSHD_DIR}/pid" ]; then
		kill $(cat "${SSHD_DIR}/pid")
	fi
}

die() {
	cleanup
	exit $1
}

TMPDIR=${TMPDIR:-/tmp}
USER=${USER:-$(whoami)}

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
	curl -L https://github.com/ethomson/poxyproxy/releases/download/v0.1.0/poxyproxy-0.1.0.jar >poxyproxy.jar
	java -jar poxyproxy.jar -d --port 8080 --credentials foo:bar >/dev/null 2>&1 &
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

if [ -z "$SKIP_GITDAEMON_TESTS" ]; then
	export GITTEST_REMOTE_URL="git://localhost/test.git"
fi

echo ""
echo "##############################################################################"
echo "## Running (offline) tests"
echo "##############################################################################"

export GITTEST_REMOTE_URL="git://localhost/test.git"
ctest -V -R libgit2_clar || die $?
unset GITTEST_REMOTE_URL

# Run the various online tests.  The "online" test suite only includes the
# default online tests that do not require additional configuration.  The
# "proxy" and "ssh" test suites require further setup.

echo ""
echo "Running proxy tests"
echo ""

export GITTEST_REMOTE_PROXY_URL="http://foo:bar@localhost:8080/"
./libgit2_clar -sonline::clone::proxy_credentials_in_url || die $?

export GITTEST_REMOTE_PROXY_URL="http://localhost:8080/"
export GITTEST_REMOTE_PROXY_USER="foo"
export GITTEST_REMOTE_PROXY_PASS="bar"
./libgit2_clar -sonline::clone::proxy_credentials_request || die $?

echo ""
echo "Running ssh tests"
echo ""

export GITTEST_REMOTE_URL="ssh://localhost:2222/$SSHD_DIR/test.git"
export GITTEST_REMOTE_USER=$USER
export GITTEST_REMOTE_SSH_KEY="$HOME/.ssh/id_rsa"
export GITTEST_REMOTE_SSH_PUBKEY="$HOME/.ssh/id_rsa.pub"
export GITTEST_REMOTE_SSH_PASSPHRASE=""
./libgit2_clar -sonline::push -sonline::clone::ssh_cert || die $?
./libgit2_clar -sonline::clone::ssh_with_paths || die $?

if [ "$TRAVIS_OS_NAME" = "linux" ]; then
	./libgit2_clar -sonline::clone::cred_callback || die $?
fi

echo ""
echo "Running credential callback tests"
echo ""

export GITTEST_REMOTE_URL="https://github.com/libgit2/non-existent"
export GITTEST_REMOTE_USER="libgit2test"
ctest -V -R libgit2_clar-cred_callback || die $?

cleanup
