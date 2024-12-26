add_library(xdiff OBJECT)

set(XDIFF_SRC git-xdiff.h
	xdiff.h
	xdiffi.c xdiffi.h
	xemit.c xemit.h
	xhistogram.c
	xinclude.h
	xmacros.h
	xmerge.c
	xpatience.c
	xprepare.c xprepare.h
	xtypes.h
	xutils.c xutils.h)

target_sources(xdiff PRIVATE ${XDIFF_SRC})
target_include_directories(xdiff SYSTEM PRIVATE
	"${PROJECT_SOURCE_DIR}/include"
	"${PROJECT_SOURCE_DIR}/src/util"
	"${PROJECT_BINARY_DIR}/src/util"
	${LIBGIT2_SYSTEM_INCLUDES}
	${LIBGIT2_DEPENDENCY_INCLUDES})

# the xdiff dependency is not (yet) warning-free, disable warnings
# as errors for the xdiff sources until we've sorted them out
if(MSVC)
        set_source_files_properties(xdiffi.c PROPERTIES COMPILE_FLAGS -WX-)
        set_source_files_properties(xemit.c PROPERTIES COMPILE_FLAGS -WX-)
        set_source_files_properties(xhistogram.c PROPERTIES COMPILE_FLAGS -WX-)
        set_source_files_properties(xmerge.c PROPERTIES COMPILE_FLAGS -WX-)
        set_source_files_properties(xutils.c PROPERTIES COMPILE_FLAGS -WX-)
        set_source_files_properties(xpatience.c PROPERTIES COMPILE_FLAGS -WX-)
else()
        set_source_files_properties(xdiffi.c PROPERTIES COMPILE_FLAGS "-Wno-sign-compare -Wno-unused-parameter")
        set_source_files_properties(xemit.c PROPERTIES COMPILE_FLAGS "-Wno-sign-compare -Wno-unused-parameter")
        set_source_files_properties(xhistogram.c PROPERTIES COMPILE_FLAGS "-Wno-sign-compare")
        set_source_files_properties(xutils.c PROPERTIES COMPILE_FLAGS "-Wno-sign-compare")
        set_source_files_properties(xpatience.c PROPERTIES COMPILE_FLAGS "-Wno-sign-compare")
endif()
