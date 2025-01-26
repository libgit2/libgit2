# fuzzers: libFuzzer and standalone fuzzing utilities

if(BUILD_FUZZERS AND NOT USE_STANDALONE_FUZZERS)
	set(CMAKE_REQUIRED_FLAGS "-fsanitize=fuzzer-no-link")
	add_c_flag(-fsanitize=fuzzer)
	add_c_flag(-fsanitize=fuzzer-no-link)
	unset(CMAKE_REQUIRED_FLAGS)
endif()

set(FUZZERS
	commit_graph
	config_file
	download_refs
	midx
	objects
	packfile
	patch_parse
	revparse)

foreach(fuzzer ${FUZZERS})
	set(fuzzer_target "${fuzzer}_fuzzer")
	set(${fuzzer}_SRC "${fuzzer}_fuzzer.c" fuzzer_utils.c ${LIBGIT2_OBJECTS})

	if(USE_STANDALONE_FUZZERS)
		list(APPEND ${fuzzer}_SRC standalone_driver.c)
	endif()

	add_executable(${fuzzer_target} ${${fuzzer}_SRC})
	target_include_directories(${fuzzer_target} PRIVATE ${LIBGIT2_INCLUDES} ${LIBGIT2_DEPENDENCY_INCLUDES})
	target_include_directories(${fuzzer_target} SYSTEM PRIVATE ${LIBGIT2_SYSTEM_INCLUDES})

	target_link_libraries(${fuzzer_target} ${LIBGIT2_SYSTEM_LIBS})

	add_test(${fuzzer_target} "${CMAKE_CURRENT_BINARY_DIR}/${fuzzer_target}" "${CMAKE_CURRENT_SOURCE_DIR}/corpora/${fuzzer}")
endforeach()
