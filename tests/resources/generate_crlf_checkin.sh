#!/usr/bin/env bash
#
# This script will generate the test corpus for CR/LF data using git;
# we created files with all possible line ending varieties (all LF, all
# CRLF, mixed, etc) on all the possible line ending configurations
# (`core.autocrlf=true`, `text=auto` in gitattributes, etc) add add them
# to git and check the added hash.  This allows us to validate that our
# configuration will match byte-for-byte the configuration that git produces.
#
# To update the test resource data, from the test resource directory:
#     git rm -r ./crlf_data/checkin_results
#     sh ./generate_crlf_checkin.sh ./crlf_data/checkin_input_files ./crlf_data/checkin_results /tmp/crlf_gitdirs
#     git add ./crlf_data/checkin_results

set -e

if [ "$1" == "" -o "$2" == "" ]; then
	echo "usage: $0 inputfiles-directory directory [tempdir]"
	exit 1
fi

input=$1
output=$2
tempdir=$3

if [ ${input:1} != "/" ]; then
	input="$PWD/$input"
fi

if [ ${output:1} != "/" ]; then
	output="$PWD/$output"
fi

if [ "${tempdir}" != "" -a "${tempdir:1}" != "/" ]; then
	tempdir="$PWD/$tempdir"
fi

set -u

create_test_data() {
	local input=$1
	local output=$2
	local tempdir=$3
	local safecrlf=$4
	local autocrlf=$5
	local attr=$6

	local destdir="${output}/safecrlf_${safecrlf},autocrlf_${autocrlf}"

	if [ "$attr" != "" ]; then
		local attrdir=`echo $attr | sed -e "s/ /,/g" | sed -e "s/=/_/g"`
		destdir="${destdir},${attrdir}"
	fi

	if [ "$tempdir" = "" ]; then
		tempdir="${output}/generate_crlf_${RANDOM}"
	else
		tempdir="${tempdir}/generate_crlf_${RANDOM}"
	fi

	echo "Generating ${destdir}"
	mkdir -p "${destdir}"
	mkdir -p "${tempdir}"

	git init "${tempdir}"
	if [ "$attr" != "" ]; then
		echo "* ${attr}" > "${tempdir}/.gitattributes"
	fi
	cp "$input"/* "${tempdir}"
	pushd "${tempdir}"
		git config core.autocrlf ${autocrlf}
		git config core.safecrlf ${safecrlf}
		for file in *
		do
			process_file "$destdir" "$file"
		done
	popd

	rm -rf "$tempdir"
}

function process_file() {
	destdir=$1
	file=$2

	rm -f "$destdir/$file.obj" "$destdir/$file.fail"

	set +e
	OUTPUT=$(git add "$file" 2>&1)
	if [ $? -ne 0 ]; then
		set -e
		touch "$destdir/$file.fail"
		if [ "${OUTPUT:0:38}" == "fatal: CRLF would be replaced by LF in" ]; then
			echo "CRLF would be replaced by LF" > "$destdir/$file.fail"
		elif [ "${OUTPUT:0:38}" == "fatal: LF would be replaced by CRLF in" ]; then
			echo "LF would be replaced by CRLF" > "$destdir/$file.fail"
		fi
	else
		OBJ=$(git ls-files -s | cut -d ' ' -f 2)
 		set -e
		echo "$OBJ" > "$destdir/$file.obj"
	fi
	rm -f .git/index
}

export LC_ALL=C

for safecrlf in true false warn; do
	for autocrlf in true false input; do
		for attr in "" text text=auto -text crlf -crlf eol=lf eol=crlf \
			"text eol=lf" "text eol=crlf" \
			"text=auto eol=lf" "text=auto eol=crlf"; do
	
			create_test_data "${input}" "${output}" "${tempdir}" \
				"${safecrlf}" "${autocrlf}" "${attr}"
		done
	done
done
