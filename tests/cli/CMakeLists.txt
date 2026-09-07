add_test(NAME cli_hash_object
	COMMAND "${CMAKE_COMMAND}"
		"-DGIT2=$<TARGET_FILE:git2_cli>"
		"-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/hash_object"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/hash_object.cmake")
