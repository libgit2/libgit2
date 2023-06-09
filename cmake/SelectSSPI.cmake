if(USE_SSPI AND WIN32)
	set(GIT_SSPI 1)
	list(APPEND LIBGIT2_SYSTEM_LIBS "crypt32" "secur32")
	list(APPEND LIBGIT2_PC_LIBS "-lcrypt32" "-lsecur32")
	
else()
	set(GIT_SSPI 0)
	add_feature_info(GSSAPI NO "SSPI not supported on this platform")
endif()
