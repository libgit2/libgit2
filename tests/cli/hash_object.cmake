# hash-object must validate non-blob objects without opening a repository.
file(MAKE_DIRECTORY "${TEST_DIR}")

function(check_hash type content expected)
	set(input "${TEST_DIR}/${type}")
	file(WRITE "${input}.in" "${content}")
	configure_file("${input}.in" "${input}" @ONLY NEWLINE_STYLE LF)

	foreach(source file stdin)
		if(source STREQUAL "stdin")
			set(argument --stdin)
		else()
			set(argument "${input}")
		endif()

		execute_process(
			COMMAND "${GIT2}" hash-object -t "${type}" "${argument}"
			INPUT_FILE "${input}"
			WORKING_DIRECTORY "${TEST_DIR}"
			RESULT_VARIABLE result
			OUTPUT_VARIABLE output
			ERROR_VARIABLE error
			OUTPUT_STRIP_TRAILING_WHITESPACE)
		if(NOT result EQUAL 0 OR NOT output STREQUAL expected)
			message(FATAL_ERROR "hash-object ${type} (${source}): ${result}: ${error}; expected ${expected}, got ${output}")
		endif()
	endforeach()
endfunction()

check_hash(blob "hello\n" "ce013625030ba8dba906f756967f9e9ca394464a")
check_hash(tree "" "4b825dc642cb6eb9a060e54bf8d69288fbee4904")
check_hash(commit "tree 4b825dc642cb6eb9a060e54bf8d69288fbee4904\nauthor A <a@example.com> 0 +0000\ncommitter A <a@example.com> 0 +0000\n\nmessage\n" "3e69154c7907f840f755c95aa0668b6d7a1beb21")
check_hash(tag "object 4b825dc642cb6eb9a060e54bf8d69288fbee4904\ntype tree\ntag example\ntagger A <a@example.com> 0 +0000\n\nmessage\n" "541cfe5a78e64e705f7fe0b87b6e858399f4fa43")

# Selecting a default hash algorithm must not bypass object validation.
foreach(type commit tag tree)
	execute_process(
		COMMAND "${GIT2}" hash-object -t "${type}" "${TEST_DIR}/blob"
		WORKING_DIRECTORY "${TEST_DIR}"
		RESULT_VARIABLE result
		OUTPUT_VARIABLE output
		ERROR_VARIABLE error)
	if(result EQUAL 0)
		message(FATAL_ERROR "hash-object accepted malformed ${type} content")
	endif()
endforeach()
