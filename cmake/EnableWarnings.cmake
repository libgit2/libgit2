INCLUDE(CheckCCompilerFlag)

function(target_enable_warning FLAG)
	string(TOUPPER -W${FLAG} UPCASE)
	string(REGEX REPLACE "[-=]" "_" UPCASE_PRETTY ${UPCASE})
	string(REGEX REPLACE "^_+" "" UPCASE_PRETTY ${UPCASE_PRETTY})
	check_c_compiler_flag(-W${FLAG} IS_${UPCASE_PRETTY}_SUPPORTED)

	if(IS_${UPCASE_PRETTY}_SUPPORTED)
		target_compile_options(TARGET PRIVATE -W${FLAG})
	endif()
endfunction()

function(target_disable_warning FLAG)
	string(TOUPPER -Wno${FLAG} UPCASE)
	string(REGEX REPLACE "[-=]" "_" UPCASE_PRETTY ${UPCASE})
	string(REGEX REPLACE "^_+" "" UPCASE_PRETTY ${UPCASE_PRETTY})
	check_c_compiler_flag(-Wno${FLAG} IS_${UPCASE_PRETTY}_SUPPORTED)

	if(IS_${UPCASE_PRETTY}_SUPPORTED)
		target_compile_options(TARGET PRIVATE -Wno${FLAG})
	endif()
endfunction()
