include(SanitizeInput)

if(USE_HTTP STREQUAL "")
	set(USE_HTTP ON)
endif()

sanitizeinput(USE_HTTP)

if(USE_HTTP STREQUAL ON)
	set(GIT_HTTP 1)
	add_feature_info(HTTP ON "HTTP transport support")
elseif(USE_HTTP STREQUAL OFF)
	# Several features only have meaning over HTTP: HTTPS, NTLM and
	# Negotiate auth. If the user did not request them explicitly, silently
	# disable them to keep configurations consistent. If they were requested
	# explicitly, error out: they cannot be built without the HTTP transport.
	foreach(_dep USE_HTTPS USE_AUTH_NTLM USE_AUTH_NEGOTIATE)
		if(${_dep} STREQUAL "")
			set(${_dep} OFF)
		else()
			set(_dep_check "${${_dep}}")
			sanitizeinput(_dep_check)
			if(_dep_check)
				message(FATAL_ERROR "${_dep}=${${_dep}} requires USE_HTTP=ON; this feature depends on the HTTP transport")
			endif()
		endif()
	endforeach()

	add_feature_info(HTTP OFF "HTTP transport support is disabled")
else()
	message(FATAL_ERROR "unknown HTTP option: ${USE_HTTP}")
endif()
