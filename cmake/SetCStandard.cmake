if("${C_STANDARD}" STREQUAL "")
	set(C_STANDARD "90")
endif()
if("${C_EXTENSIONS}" STREQUAL "")
	set(C_EXTENSIONS OFF)
endif()

if(${C_STANDARD} MATCHES "^[Cc].*")
	string(REGEX REPLACE "^[Cc]" "" C_STANDARD ${C_STANDARD})
endif()

if(${C_STANDARD} MATCHES ".*-strict$")
	string(REGEX REPLACE "-strict$" "" C_STANDARD ${C_STANDARD})
	set(C_EXTENSIONS OFF)

	add_feature_info("C Standard" ON "C${C_STANDARD} (strict)")
else()
	add_feature_info("C Standard" ON "C${C_STANDARD}")
endif()

function(set_c_standard project)
	set_target_properties(${project} PROPERTIES C_STANDARD ${C_STANDARD})
	set_target_properties(${project} PROPERTIES C_EXTENSIONS ${C_EXTENSIONS})
endfunction()
