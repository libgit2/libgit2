# examples: code usage examples of libgit2

add_executable(lg2_examples)

set(EXAMPLES_SRC
	add.c
	args.c
	args.h
	blame.c
	cat-file.c
	checkout.c
	clone.c
	commit.c
	common.c
	common.h
	config.c
	describe.c
	diff.c
	fetch.c
	for-each-ref.c
	general.c
	index-pack.c
	init.c
	lg2.c
	log.c
	ls-files.c
	ls-remote.c
	merge.c
	push.c
	remote.c
	rev-list.c
	rev-parse.c
	show-index.c
	stash.c
	status.c
	tag.c)

# Ensure that we do not use deprecated functions internally
add_definitions(-DGIT_DEPRECATE_HARD)

target_sources(lg2_examples PRIVATE ${EXAMPLES_SRC})

target_include_directories(lg2_examples PRIVATE ${LIBGIT2_INCLUDES} ${LIBGIT2_DEPENDENCY_INCLUDES})
target_include_directories(lg2_examples SYSTEM PRIVATE ${LIBGIT2_SYSTEM_INCLUDES})

if(WIN32 OR ANDROID)
	target_link_libraries(lg2_examples libgit2package)
else()
	target_link_libraries(lg2_examples libgit2package pthread)
endif()
