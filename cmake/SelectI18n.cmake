include(SanitizeInput)

find_package(IntlIconv)

if(USE_I18N STREQUAL "" AND NOT USE_ICONV STREQUAL "")
	sanitizeinput(USE_ICONV)
	set(USE_I18N "${USE_ICONV}")
endif()

if(USE_I18N STREQUAL "")
	set(USE_I18N ON)
endif()

sanitizeinput(USE_I18N)

if(USE_I18N)
	if(USE_I18N STREQUAL ON)
		if(ICONV_FOUND)
			set(USE_I18N "iconv")
		else()
                        message(FATAL_ERROR "Unable to detect internationalization support")
                endif()
        endif()

	if(USE_I18N STREQUAL "iconv")
	else()
		message(FATAL_ERROR "unknown internationalization backend: ${USE_I18N}")
	endif()

	list(APPEND LIBGIT2_SYSTEM_INCLUDES ${ICONV_INCLUDE_DIR})
	list(APPEND LIBGIT2_SYSTEM_LIBS ${ICONV_LIBRARIES})
	list(APPEND LIBGIT2_PC_LIBS ${ICONV_LIBRARIES})

        set(GIT_I18N 1)
        set(GIT_I18N_ICONV 1)
        add_feature_info("Internationalization" ON "using ${USE_I18N}")
else()
        set(GIT_I18N 0)
        add_feature_info("Internationalization" OFF "internationalization support is disabled")
endif()
