disable_warnings(sign-compare)
disable_warnings(unused-parameter)

set(REFTABLE_SOURCES
	basics.c
	basics.h
	block.c
	block.h
	blocksource.c
	blocksource.h
	compat.c
	compat.h
	iter.c
	iter.h
	merged.c
	merged.h
	pq.c
	pq.h
	publicbasics.c
	reader.c
	reader.h
	record.c
	record.h
	refname.c
	refname.h
	reftable.c
	stack.c
	stack.h
	strbuf.c
	strbuf.h
	system.h
	tree.c
	tree.h
	writer.c
	writer.h
	zlib-compat.c)
add_library(reftable STATIC ${REFTABLE_SOURCES})
target_compile_definitions(reftable PUBLIC REFTABLE_STANDALONE)
target_include_directories(reftable PUBLIC include)
