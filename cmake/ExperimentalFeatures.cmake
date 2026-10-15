# Experimental feature support for libgit2 - developers can opt in to
# experimental functionality. (Though there are currently no
# experimental options - SHA256 was first implemented as an experimental
# option.)
#
# When experimental functionality is enabled, we set both a cmake flag
# *and* a compile definition. The cmake flag is used to generate
# `experimental.h`, which will be installed by a `make install`.
# But the compile definition is used by the libgit2 sources to detect
# the functionality at library build time. This allows us to have an
# in-tree `experimental.h` with *no* experiments enabled. This lets
# us support users who build without cmake and cannot generate the
# `experimental.h` file.

if(EXPERIMENTAL)
	set(LIBGIT2_FILENAME "${LIBGIT2_FILENAME}-experimental")
endif()
