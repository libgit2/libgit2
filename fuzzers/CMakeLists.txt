IF(BUILD_FUZZERS AND NOT USE_STANDALONE_FUZZERS)
	ADD_C_FLAG(-fsanitize=fuzzer)
ENDIF ()

FILE(GLOB SRC_FUZZ RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *_fuzzer.c)
FOREACH(fuzz_target_src ${SRC_FUZZ})
	STRING(REPLACE ".c" "" fuzz_target_name ${fuzz_target_src})
	STRING(REPLACE "_fuzzer" "" fuzz_name ${fuzz_target_name})

	SET(${fuzz_target_name}_SOURCES ${fuzz_target_src})
	IF(USE_STANDALONE_FUZZERS)
		LIST(APPEND ${fuzz_target_name}_SOURCES "standalone_driver.c")
	ENDIF()
	ADD_EXECUTABLE(${fuzz_target_name} ${${fuzz_target_name}_SOURCES} $<TARGET_OBJECTS:git2internal>)
	SET_TARGET_PROPERTIES(${fuzz_target_name} PROPERTIES C_STANDARD 90)
	# Workaround as CMake v3.5 doesn't yet allow to link against
	# object libraries directly.
	TARGET_INCLUDE_DIRECTORIES(${fuzz_target_name} PRIVATE $<TARGET_PROPERTY:git2internal,INCLUDE_DIRECTORIES>)
	TARGET_LINK_LIBRARIES(${fuzz_target_name} $<TARGET_PROPERTY:git2,LINK_LIBRARIES>)

	ADD_TEST(${fuzz_target_name} "${CMAKE_CURRENT_BINARY_DIR}/${fuzz_target_name}" "${CMAKE_CURRENT_SOURCE_DIR}/corpora/${fuzz_name}")
ENDFOREACH()
