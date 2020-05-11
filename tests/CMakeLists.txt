function(add_clar_test name)
	if(NOT USE_LEAK_CHECKER STREQUAL "OFF")
		add_test(${name} "${libgit2_SOURCE_DIR}/script/${USE_LEAK_CHECKER}.sh" "${libgit2_BINARY_DIR}/libgit2_tests" ${ARGN})
	else()
		add_test(${name} "${libgit2_BINARY_DIR}/libgit2_tests" ${ARGN})
	endif()
endfunction(add_clar_test)

add_subdirectory(libgit2)
