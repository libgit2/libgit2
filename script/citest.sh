#!/bin/sh

set -x

# If this platform doesn't support test execution, bail out now
if [ -n "$SKIP_TESTS" ]; then
	exit $?
fi

if [ ! -d _build ]; then
	echo "no _build dir found; you should run cibuild.sh first"
	exit 1
fi
cd _build

# Should we ask Travis to cache this file?
curl -L https://github.com/ethomson/poxyproxy/releases/download/v0.1.0/poxyproxy-0.1.0.jar >poxyproxy.jar || exit $?
# Run this early so we know it's ready by the time we need it
java -jar poxyproxy.jar -d --port 8080 --credentials foo:bar &

# Create a test repo which we can use for the online::push tests
mkdir "$HOME"/_temp
git init --bare "$HOME"/_temp/test.git
git daemon --listen=localhost --export-all --enable=receive-pack --base-path="$HOME"/_temp "$HOME"/_temp 2>/dev/null &
export GITTEST_REMOTE_URL="git://localhost/test.git"

# Run the test suite
ctest -V -R libgit2_clar || exit $?

# Now that we've tested the raw git protocol, let's set up ssh to we
# can do the push tests over it

killall git-daemon

# Set up sshd
mkdir ~/sshd/
cat >~/sshd/sshd_config<<-EOF
	Port 2222
	ListenAddress 0.0.0.0
	Protocol 2
	HostKey ${HOME}/sshd/id_rsa
	PidFile ${HOME}/sshd/pid
	RSAAuthentication yes
	PasswordAuthentication yes
	PubkeyAuthentication yes
	ChallengeResponseAuthentication no
	# Required here as sshd will simply close connection otherwise
	UsePAM no
EOF
ssh-keygen -t rsa -f ~/sshd/id_rsa -N "" -q
/usr/sbin/sshd -f ~/sshd/sshd_config

# Set up keys
ssh-keygen -t rsa -f ~/.ssh/id_rsa -N "" -q
cat ~/.ssh/id_rsa.pub >>~/.ssh/authorized_keys
while read algorithm key comment; do
	echo "[localhost]:2222 $algorithm $key" >>~/.ssh/known_hosts
done <~/sshd/id_rsa.pub

# Get the fingerprint for localhost and remove the colons so we can parse it as
# a hex number. The Mac version is newer so it has a different output format.
if [ "$TRAVIS_OS_NAME" = "osx" ]; then
	export GITTEST_REMOTE_SSH_FINGERPRINT=$(ssh-keygen -E md5 -F '[localhost]:2222' -l | tail -n 1 | cut -d ' ' -f 3 | cut -d : -f2- | tr -d :)
else
	export GITTEST_REMOTE_SSH_FINGERPRINT=$(ssh-keygen -F '[localhost]:2222' -l | tail -n 1 | cut -d ' ' -f 2 | tr -d ':')
fi

# Use the SSH server
export GITTEST_REMOTE_URL="ssh://localhost:2222/$HOME/_temp/test.git"
export GITTEST_REMOTE_USER=$USER
export GITTEST_REMOTE_SSH_KEY="$HOME/.ssh/id_rsa"
export GITTEST_REMOTE_SSH_PUBKEY="$HOME/.ssh/id_rsa.pub"
export GITTEST_REMOTE_SSH_PASSPHRASE=""
ctest -V -R libgit2_clar-ssh || exit $?

# Use the proxy we started at the beginning
export GITTEST_REMOTE_PROXY_URL="localhost:8080"
export GITTEST_REMOTE_PROXY_USER="foo"
export GITTEST_REMOTE_PROXY_PASS="bar"
ctest -V -R libgit2_clar-proxy_credentials || exit $?

kill $(cat "$HOME/sshd/pid")
