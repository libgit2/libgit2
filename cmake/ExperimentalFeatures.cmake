if(EXPERIMENTAL_SHA256)
	add_feature_info("SHA256 API" ON "experimental SHA256 APIs")

	set(EXPERIMENTAL 1)
	set(GIT_EXPERIMENTAL_SHA256 1)
else()
	add_feature_info("SHA256 API" OFF "experimental SHA256 APIs")
endif()

if(EXPERIMENTAL)
	set(LIBGIT2_FILENAME "${LIBGIT2_FILENAME}-experimental")
endif()
