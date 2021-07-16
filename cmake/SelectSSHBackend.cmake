# Auto-select SSH backend

# We try to find any packages our backends might use
FIND_PKGLIBRARIES(LIBSSH2 libssh2)
FIND_PKGLIBRARIES(LIBSSH libssh)

IF (USE_SSH STREQUAL ON)
	IF (LIBSSH2_FOUND)
		SET(SSH_BACKEND "libssh2")
	ELSEIF(LIBSSH_FOUND)
		SET(SSH_BACKEND "libssh")
	ELSE()
		MESSAGE("Unable to autodetect a usable SSH backend."
			"Please pass the backend name explicitly (-DUSE_SSH=backend)")
	ENDIF()
ELSEIF(USE_SSH)
	# SSH backend was explicitly set
	SET(SSH_BACKEND ${USE_SSH})
ELSE()
	SET(SSH_BACKEND NO)
ENDIF()

IF(SSH_BACKEND)
	# Check that we can find what's required for the selected backend
	IF (SSH_BACKEND STREQUAL "libssh2")
		IF (NOT LIBSSH2_FOUND)
			MESSAGE(FATAL_ERROR "LIBSSH2 not found. Set CMAKE_PREFIX_PATH if it is installed outside of the default search path.")
		ENDIF()

		SET(GIT_LIBSSH2 1)
		LIST(APPEND LIBGIT2_SYSTEM_INCLUDES ${LIBSSH2_INCLUDE_DIRS})
		LIST(APPEND LIBGIT2_LIBS ${LIBSSH2_LIBRARIES})
		LIST(APPEND LIBGIT2_PC_REQUIRES "libssh2")

		CHECK_LIBRARY_EXISTS("${LIBSSH2_LIBRARIES}" libssh2_userauth_publickey_frommemory "${LIBSSH2_LIBRARY_DIRS}" HAVE_LIBSSH2_MEMORY_CREDENTIALS)
		IF (HAVE_LIBSSH2_MEMORY_CREDENTIALS)
			SET(GIT_SSH_MEMORY_CREDENTIALS 1)
		ENDIF()
	ELSEIF(SSH_BACKEND STREQUAL "libssh")
		IF (NOT LIBSSH_FOUND)
			MESSAGE(FATAL_ERROR "libssh not found. Set CMAKE_PREFIX_PATH if it is installed outside of the default search path.")
		ENDIF()

		SET(GIT_LIBSSH 1)
		LIST(APPEND LIBGIT2_SYSTEM_INCLUDES ${LIBSSH_INCLUDE_DIRS})
		LIST(APPEND LIBGIT2_LIBS ${LIBSSH_LIBRARIES})
		LIST(APPEND LIBGIT2_PC_REQUIRES "libssh")
	ELSE()
		MESSAGE(FATAL_ERROR "Asked for backend ${SSH_BACKEND} but it wasn't found")
	ENDIF()

	SET(GIT_SSH 1)
	ADD_FEATURE_INFO(SSH GIT_SSH "SSH transport support, using ${SSH_BACKEND}")
ELSE()
	SET(GIT_SSH 0)
	ADD_FEATURE_INFO(SSH NO "")
ENDIF()
