include_directories(${LIBGIT2_INCLUDES})
include_directories(SYSTEM ${LIBGIT2_SYSTEM_INCLUDES})

file(GLOB LG2_SOURCES *.c *.h)
add_executable(lg2 ${LG2_SOURCES})
target_libgit2_warnings(lg2)
set_target_properties(lg2 PROPERTIES C_STANDARD 90)

# Ensure that we do not use deprecated functions internally
add_definitions(-DGIT_DEPRECATE_HARD)

if(WIN32 OR ANDROID)
	target_link_libraries(lg2 git2)
else()
	target_link_libraries(lg2 git2 pthread)
endif()
