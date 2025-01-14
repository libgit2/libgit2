# variables that benchmark tests can set
#

set -eo pipefail

#
# command-line parsing
#

usage() { echo "usage: $(basename "$0") [--cli <path>] [--baseline-cli <path>] [--output-style <style>] [--json <path>] [--profile] [--flamegraph <path>]"; }

NEXT=
BASELINE_CLI=
TEST_CLI="git"
SHOW_OUTPUT=
JSON=
PROFILE=
FLAMEGRAPH=

if [ "$CI" != "" -a -t 1 ]; then
	OUTPUT_STYLE="color"
else
	OUTPUT_STYLE="auto"
fi

HELP_GIT_REMOTE="https://github.com/git/git"
HELP_LINUX_REMOTE="https://github.com/torvalds/linux"
HELP_RESOURCE_REPO="https://github.com/libgit2/benchmark-resources"

BENCHMARK_DIR=${BENCHMARK_DIR:=$(dirname "$0")}

#
# parse the arguments to the outer script that's including us; these are arguments that
# the `benchmark.sh` passes (or that a user could specify when running an individual test)
#

for a in "$@"; do
	if [ "${NEXT}" = "cli" ]; then
		TEST_CLI="${a}"
		NEXT=
	elif [ "${NEXT}" = "baseline-cli" ]; then
		BASELINE_CLI="${a}"
		NEXT=
	elif [ "${NEXT}" = "output-style" ]; then
		OUTPUT_STYLE="${a}"
		NEXT=
	elif [ "${NEXT}" = "json" ]; then
		JSON="${a}"
		NEXT=
	elif [ "${NEXT}" = "flamegraph" ]; then
		FLAMEGRAPH="${a}"
		NEXT=
	elif [ "${a}" = "-c" ] || [ "${a}" = "--cli" ]; then
		NEXT="cli"
	elif [[ "${a}" == "-c"* ]]; then
		TEST_CLI="${a/-c/}"
	elif [ "${a}" = "-b" ] || [ "${a}" = "--baseline-cli" ]; then
		NEXT="baseline-cli"
	elif [[ "${a}" == "-b"* ]]; then
		BASELINE_CLI="${a/-b/}"
	elif [ "${a}" == "--output-style" ]; then
		NEXT="output-style"
	elif [ "${a}" = "--show-output" ]; then
		SHOW_OUTPUT=1
		OUTPUT_STYLE=
	elif [ "${a}" = "-j" ] || [ "${a}" = "--json" ]; then
		NEXT="json"
	elif [[ "${a}" == "-j"* ]]; then
                JSON="${a/-j/}"
	elif [ "${a}" = "-p" ] || [ "${a}" = "--profile" ]; then
		PROFILE=1
	elif [ "${a}" = "-F" ] || [ "${a}" = "--flamegraph" ]; then
		NEXT="flamegraph"
	elif [[ "${a}" == "-F"* ]]; then
                FLAMEGRAPH="${a/-F/}"
	else
                echo "$(basename "$0"): unknown option: ${a}" 1>&2
		usage 1>&2
		exit 1
	fi
done

if [ "${NEXT}" != "" ]; then
	echo "$(basename "$0"): option requires a value: --${NEXT}" 1>&2
        usage 1>&2
        exit 1
fi

fullpath() {
	FULLPATH="${1}"
	if [[ "$(uname -s)" == "MINGW"* ]]; then FULLPATH="$(cygpath -u "${1}")"; fi

	if [[ "${FULLPATH}" != *"/"* ]]; then
                FULLPATH="$(which "${FULLPATH}")"
                if [ "$?" != "0" ]; then exit 1; fi
	else
		FULLPATH="$(cd "$(dirname "${FULLPATH}")" && pwd)/$(basename "${FULLPATH}")"
	fi

	if [[ "$(uname -s)" == "MINGW"* ]]; then FULLPATH="$(cygpath -w "${FULLPATH}")"; fi
	echo "${FULLPATH}"
}

resources_dir() {
	cd "$(dirname "$0")/../resources" && pwd
}

temp_dir() {
	if [ "$(uname -s)" == "Darwin" ]; then
		mktemp -dt libgit2_bench
	else
		mktemp -dt libgit2_bench.XXXXXXX
	fi
}

create_prepare_script() {
	# add some functions for users to use in preparation
	cat >> "${SANDBOX_DIR}/prepare.sh" << EOF
	set -e

	SANDBOX_DIR="${SANDBOX_DIR}"
	RESOURCES_DIR="$(resources_dir)"

	create_text_file() {
		FILENAME="\${1}"
		SIZE="\${2}"

		if [ "\${FILENAME}" = "" ]; then
			echo "usage: create_text_file <name> [size]" 1>&2
			exit 1
		fi

		if [ "\${SIZE}" = "" ]; then
			SIZE="1024"
		fi

		if [[ "\$(uname -s)" == "MINGW"* ]]; then
			EOL="\r\n"
			EOL_LEN="2"
			CONTENTS="This is a reproducible text file. (With Unix line endings.)\n"
			CONTENTS_LEN="60"
		else
			EOL="\n"
			EOL_LEN="1"
			CONTENTS="This is a reproducible text file. (With DOS line endings.)\r\n"
			CONTENTS_LEN="60"
		fi

		rm -f "\${FILENAME:?}"
		touch "\${FILENAME}"

		if [ "\${SIZE}" -ge "\$((\${CONTENTS_LEN} + \${EOL_LEN}))" ]; then
			SIZE="\$((\${SIZE} - \${CONTENTS_LEN}))"
			COUNT="\$(((\${SIZE} - \${EOL_LEN}) / \${CONTENTS_LEN}))"

			if [ "\${SIZE}" -gt "\${EOL_LEN}" ]; then
				dd if="\${FILENAME}" of="\${FILENAME}" bs="\${CONTENTS_LEN}" seek=1 count="\${COUNT}" 2>/dev/null
			fi

			SIZE="\$((\${SIZE} - (\${COUNT} * \${CONTENTS_LEN})))"
                fi

		while [ "\${SIZE}" -gt "\${EOL_LEN}" ]; do
			echo -ne "." >> "\${FILENAME}"
			SIZE="\$((\${SIZE} - 1))"
		done

		if [ "\${SIZE}" = "\${EOL_LEN}" ]; then
			echo -ne "\${EOL}" >> "\${FILENAME}"
			SIZE="\$((\${SIZE} - \${EOL_LEN}))"
		else
			while [ "\${SIZE}" -gt "0" ]; do
				echo -ne "." >> "\${FILENAME}"
				SIZE="\$((\${SIZE} - 1))"
			done
		fi
	}

	create_random_file() {
		FILENAME="\${1}"
		SIZE="\${2}"

		if [ "\${FILENAME}" = "" ]; then
			echo "usage: create_random_file <name> [size]" 1>&2
			exit 1
		fi

		if [ "\${SIZE}" = "" ]; then
			SIZE="1024"
		fi

		dd if="/dev/urandom" of="\${FILENAME}" bs="\${SIZE}" count=1 2>/dev/null
	}

	flush_disk_cache() {
		if [ "\$(uname -s)" = "Darwin" ]; then
			sync && sudo purge
		elif [ "\$(uname -s)" = "Linux" ]; then
			sync && echo 3 | sudo tee /proc/sys/vm/drop_caches >/dev/null
		elif [[ "\$(uname -s)" == "MINGW"* ]]; then
			PurgeStandbyList
		fi
	}

	sandbox() {
		RESOURCE="\${1}"

		if [ "\${RESOURCE}" = "" ]; then
			echo "usage: sandbox <path>" 1>&2
			exit 1
		fi

		if [ ! -d "\${RESOURCES_DIR}/\${RESOURCE}" ]; then
			echo "sandbox: the resource \"\${RESOURCE}\" does not exist"
			exit 1
		fi

		rm -rf "\${SANDBOX_DIR:?}/\${RESOURCE}"
		cp -R "\${RESOURCES_DIR}/\${RESOURCE}" "\${SANDBOX_DIR}/"
	}

	sandbox_resource() {
		RESOURCE="\${1}"

		if [ "\${RESOURCE}" = "" ]; then
			echo "usage: sandbox_resource <path>" 1>&2
			exit 1
		fi

		RESOURCE_UPPER=\$(echo "\${RESOURCE}" | tr '[:lower:]' '[:upper:]' | sed -e "s/-/_/g")
		RESOURCE_PATH=\$(eval echo "\\\${BENCHMARK_\${RESOURCE_UPPER}_PATH}")

		if [ "\${RESOURCE_PATH}" = "" -a "\${BENCHMARK_RESOURCES_PATH}" != "" ]; then
			RESOURCE_PATH="\${BENCHMARK_RESOURCES_PATH}/\${RESOURCE}"
		fi

		if [ ! -f "\${RESOURCE_PATH}" ]; then
			echo "sandbox: the resource \"\${RESOURCE}\" does not exist"
			exit 1
		fi

		rm -rf "\${SANDBOX_DIR:?}/\${RESOURCE}"
		cp -R "\${RESOURCE_PATH}" "\${SANDBOX_DIR}/\${RESOURCE}"
	}

	sandbox_repo() {
		RESOURCE="\${1}"

		sandbox "\${RESOURCE}"

		if [ -d "\${SANDBOX_DIR}/\${RESOURCE}/.gitted" ]; then
			mv "\${SANDBOX_DIR}/\${RESOURCE}/.gitted" "\${SANDBOX_DIR}/\${RESOURCE}/.git";
		fi
		if [ -f "\${SANDBOX_DIR}/\${RESOURCE}/gitattributes" ]; then
			mv "\${SANDBOX_DIR}/\${RESOURCE}/gitattributes" "\${SANDBOX_DIR}/\${RESOURCE}/.gitattributes";
		fi
		if [ -f "\${SANDBOX_DIR}/\${RESOURCE}/gitignore" ]; then
			mv "\${SANDBOX_DIR}/\${RESOURCE}/gitignore" "\${SANDBOX_DIR}/\${RESOURCE}/.gitignore";
		fi
	}

	clone_repo() {
		REPO="\${1}"

		if [ "\${REPO}" = "" ]; then
			echo "usage: clone_repo <repo>" 1>&2
			exit 1
		fi

		REPO_UPPER=\$(echo "\${REPO}" | tr '[:lower:]' '[:upper:]')
		REPO_URL=\$(eval echo "\\\${BENCHMARK_\${REPO_UPPER}_PATH}")

		if [ "\${REPO_URL}" = "" ]; then
			echo "\$0: unknown repository '\${REPO}'" 1>&2
			exit 1
		fi

		rm -rf "\${SANDBOX_DIR:?}/\${REPO}"
		git clone "\${REPO_URL}" "\${SANDBOX_DIR}/\${REPO}"
	}

	cd "\${SANDBOX_DIR}"
EOF

	if [ "${PREPARE}" != "" ]; then
		echo "" >> "${SANDBOX_DIR}/prepare.sh"
		echo "${PREPARE}" >> "${SANDBOX_DIR}/prepare.sh"
	fi

	echo "${SANDBOX_DIR}/prepare.sh"
}

start_dir() {
	if [[ "${CHDIR}" = "/"* ]]; then
		START_DIR="${CHDIR}"
	elif [ "${CHDIR}" != "" ]; then
		START_DIR="${SANDBOX_DIR}/${CHDIR}"
	else
		START_DIR="${SANDBOX_DIR}"
	fi

	echo "${START_DIR}"
}

create_run_script() {
	SCRIPT_NAME="${1}"; shift
	CLI_PATH="${1}"; shift

	START_DIR=$(start_dir)

	# our run script starts by chdir'ing to the sandbox or repository directory
	echo -n "cd \"${START_DIR}\" && \"${CLI_PATH}\"" >> "${SANDBOX_DIR}/${SCRIPT_NAME}.sh"

	for a in "$@"; do
		echo -n " \"${a}\"" >> "${SANDBOX_DIR}/${SCRIPT_NAME}.sh"
	done

	echo "" >> "${SANDBOX_DIR}/${SCRIPT_NAME}.sh"

	echo "${SANDBOX_DIR}/${SCRIPT_NAME}.sh"
}

parse_arguments() {
	NEXT=

	# this test should run the given command in preparation of the tests
	# this preparation script will be run _after_ repository creation and
	# _before_ flushing the disk cache
	PREPARE=

	# this test should run within the given directory; this is a
	# relative path beneath the sandbox directory.
	CHDIR=

	# this test should run `n` warmups
	WARMUP=0

	if [ "$*" = "" ]; then
		gitbench_usage 1>&2
		exit 1
	fi

	for a in "$@"; do
		if [ "${NEXT}" = "warmup" ]; then
			WARMUP="${a}"
			NEXT=
		elif [ "${NEXT}" = "prepare" ]; then
			PREPARE="${a}"
			NEXT=
		elif [ "${NEXT}" = "chdir" ]; then
			CHDIR="${a}"
			NEXT=
		elif [ "${a}" = "--warmup" ]; then
			NEXT="warmup"
		elif [ "${a}" = "--prepare" ]; then
			NEXT="prepare"
		elif [ "${a}" = "--chdir" ]; then
			NEXT="chdir"
		elif [[ "${a}" == "--" ]]; then
			shift
			break
		elif [[ "${a}" == "--"* ]]; then
			echo "unknown argument: \"${a}\"" 1>&2
			gitbench_usage 1>&2
			exit 1
		else
			break
		fi

		shift
	done

	if [ "${NEXT}" != "" ]; then
		echo "$(basename "$0"): option requires a value: --${NEXT}" 1>&2
		gitbench_usage 1>&2
		exit 1
	fi

	echo "PREPARE=\"${PREPARE}\""
	echo "CHDIR=\"${CHDIR}\""
	echo "WARMUP=\"${WARMUP}\""

	echo -n "GIT_ARGUMENTS=("

	for arg in $@; do
		echo -n " \"${arg}\""
	done
	echo " )"
}

gitbench_usage() { echo "usage: gitbench command..."; }

exec_profiler() {
	if [ "${BASELINE_CLI}" != "" ]; then
		echo "$0: baseline is not supported in profiling mode" 1>&2
		exit 1
	fi

	if [ "${SHOW_OUTPUT}" != "" ]; then
		echo "$0: show-output is not supported in profiling mode" 1>&2
		exit 1
	fi

	if [ "$JSON" != "" ]; then
		echo "$0: json is not supported in profiling mode" 1>&2
		exit 1
	fi

	SYSTEM=$(uname -s)

	TEST_CLI_PATH=$(fullpath "${TEST_CLI}")
	START_DIR=$(start_dir)

	if [ "${SYSTEM}" = "Linux" ]; then
		if [ "${OUTPUT_STYLE}" = "color" ]; then
			COLOR_ARG="always"
		elif [ "${OUTPUT_STYLE}" = "none" ]; then
			COLOR_ARG="never"
		elif [ "${OUTPUT_STYLE}" = "auto" ]; then
			COLOR_ARG="auto"
		else
			echo "$0: unknown output-style option" 1>&2
			exit 1
		fi

		bash "${PREPARE_SCRIPT}"
		( cd "${START_DIR}" && perf record -F 999 -a -g -o "${SANDBOX_DIR}/perf.data" -- "${TEST_CLI_PATH}" "${GIT_ARGUMENTS[@]}" )

		# we may not have samples if the process exited quickly
		SAMPLES=$(perf report -D -i "${SANDBOX_DIR}/perf.data" | { grep "RECORD_SAMPLE" || test $? = 1; } | wc -l)

		if [ "${SAMPLES}" = "0" ]; then
			echo "$0: no profiling samples created" 1>&2
			exit 3
		fi

		if [ "${FLAMEGRAPH}" = "" ]; then
			perf report --stdio --stdio-color "${COLOR_ARG}" -i "${SANDBOX_DIR}/perf.data"
		else
			perf script -i "${SANDBOX_DIR}/perf.data" | "${BENCHMARK_DIR}/_script/flamegraph/stackcollapse-perf.pl" > "${SANDBOX_DIR}/perf.data.folded"
			perl "${BENCHMARK_DIR}/_script/flamegraph/flamegraph.pl" -title "" -colors "libgit2" -bgcolors "none" "${SANDBOX_DIR}/perf.data.folded" > "${FLAMEGRAPH}"
		fi
	else
		# macos - requires system integrity protection is disabled :(
		# dtrace -s "bash ${TEST_RUN_SCRIPT}" -o filename -n "profile-997 /execname == \"${TEST_CLI}\"/ { @[ustack(100)] = count(); }"
		echo "$0: profiling is not supported on ${SYSTEM}" 1>&2
		exit 4
	fi
}

exec_hyperfine() {
	if [ "$FLAMEGRAPH" != "" ]; then
		echo "$0: flamegraph is not supported in standard mode" 1>&2
		exit 1
	fi

	if [ "${BASELINE_CLI}" != "" ]; then
		BASELINE_CLI_PATH=$(fullpath "${BASELINE_CLI}")
		BASELINE_RUN_SCRIPT=$(create_run_script "baseline" "${BASELINE_CLI_PATH}" "${GIT_ARGUMENTS[@]}")
	fi

	TEST_CLI_PATH=$(fullpath "${TEST_CLI}")
	TEST_RUN_SCRIPT=$(create_run_script "test" "${TEST_CLI_PATH}" "${GIT_ARGUMENTS[@]}")

	ARGUMENTS=("--prepare" "bash ${PREPARE_SCRIPT}" "--warmup" "${WARMUP}")

	if [ "${OUTPUT_STYLE}" != "" ]; then
		ARGUMENTS+=("--style" "${OUTPUT_STYLE}")
	fi

	if [ "${SHOW_OUTPUT}" != "" ]; then
		ARGUMENTS+=("--show-output")
	fi

	if [ "$JSON" != "" ]; then
		ARGUMENTS+=("--export-json" "${JSON}")
	fi

	if [ "${BASELINE_CLI}" != "" ]; then
		ARGUMENTS+=("-n" "${BASELINE_CLI} ${GIT_ARGUMENTS[*]}" "bash ${BASELINE_RUN_SCRIPT}")
	fi

	ARGUMENTS+=("-n" "${TEST_CLI} ${GIT_ARGUMENTS[*]}" "bash ${TEST_RUN_SCRIPT}")

	hyperfine "${ARGUMENTS[@]}"
}

#
# this is the function that the outer script calls to actually do the sandboxing and
# invocation of hyperfine.
#
gitbench() {
	eval $(parse_arguments "$@")

	# sanity check

	for a in "${SANDBOX[@]}"; do
		if [ ! -d "$(resources_dir)/${a}" ]; then
			echo "$0: no resource '${a}' found" 1>&2
			exit 1
		fi
	done

	# set up our sandboxing

	SANDBOX_DIR="$(temp_dir)"
	PREPARE_SCRIPT="$(create_prepare_script)"

	if [ "${PROFILE}" != "" ]; then
		exec_profiler
	else
		exec_hyperfine
	fi

#	rm -rf "${SANDBOX_DIR:?}"
}

# helper script to give useful error messages about configuration
needs_repo() {
	REPO="${1}"

	if [ "${REPO}" = "" ]; then
		echo "usage: needs_repo <repo>" 1>&2
		exit 1
	fi

	REPO_UPPER=$(echo "${REPO}" | tr '[:lower:]' '[:upper:]')
	REPO_PATH=$(eval echo "\${BENCHMARK_${REPO_UPPER}_PATH}")
	REPO_REMOTE_URL=$(eval echo "\${HELP_${REPO_UPPER}_REMOTE}")

	if [ "${REPO_PATH}" = "" ]; then
		echo "$0: '${REPO}' repository not configured" 1>&2
		echo "" 1>&2
		echo "This benchmark needs an on-disk '${REPO}' repository. First, clone the" 1>&2
		echo "remote repository ('${REPO_REMOTE_URL}') locally then set" 1>&2
		echo "the 'BENCHMARK_${REPO_UPPER}_PATH' environment variable to the path that" 1>&2
		echo "contains the repository locally, then run this benchmark again." 1>&2
		exit 2
	fi
}

# helper script to give useful error messages about configuration
needs_resource() {
	RESOURCE="${1}"

	if [ "${RESOURCE}" = "" ]; then
		echo "usage: needs_resource <resource>" 1>&2
		exit 1
	fi

	RESOURCE_UPPER=$(echo "${RESOURCE}" | tr '[:lower:]' '[:upper:]' | sed -e "s/-/_/g")
	RESOURCE_PATH=$(eval echo "\${BENCHMARK_${RESOURCE_UPPER}_PATH}")

	if [ "${RESOURCE_PATH}" = "" -a "${BENCHMARK_RESOURCES_PATH}" != "" ]; then
		RESOURCE_PATH="${BENCHMARK_RESOURCES_PATH}/${RESOURCE}"
	fi

	if [ "${RESOURCE_PATH}" = "" ]; then
		echo "$0: '${RESOURCE}' resource path not configured" 1>&2
		echo "" 1>&2
		echo "This benchmark needs an on-disk resource named '${RESOURCE}'." 1>&2
		echo "First, clone the additional benchmark resources locally (from" 1>&2
		echo "'${HELP_RESOURCE_REPO}'), then set the" 1>& 2
		echo "'BENCHMARK_RESOURCES_PATH' environment variable to the path that" 1>&2
		echo "contains the resources locally, then run this benchmark again." 1>&2
		exit 2
	fi
}
