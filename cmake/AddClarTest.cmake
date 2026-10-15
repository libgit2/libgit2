function(ADD_CLAR_TEST project name)
        if(NOT DEBUG_LEAK_CHECKER STREQUAL "OFF" AND
           NOT DEBUG_LEAK_CHECKER STREQUAL "" AND
	   NOT DEBUG_LEAK_CHECKER STREQUAL "win32")
                add_test(${name} "${PROJECT_SOURCE_DIR}/script/${DEBUG_LEAK_CHECKER}.sh" "${PROJECT_BINARY_DIR}/${project}" ${ARGN})
        else()
                add_test(${name} "${PROJECT_BINARY_DIR}/${project}" ${ARGN})
        endif()
endfunction(ADD_CLAR_TEST)
