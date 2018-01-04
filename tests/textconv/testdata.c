//
//  testdata.c
//  libgit2_clar
//
//  Created by Local Administrator on 12/29/17.
//

#include "testdata.h"

const char* getTestCSV(void)
{
	return
		"one,two,three,four,five\n"
		"1,2,3,4,5\n"
		"\"hello, people\",goodbye,mr.,chips,99\n";
}

const char* getTestYAML(void)
{
	return
		"-\n"
		"  - one\n"
		"  - two\n"
		"  - three\n"
		"  - four\n"
		"  - five\n"
		"-\n"
		"  - 1\n"
		"  - 2\n"
		"  - 3\n"
		"  - 4\n"
		"  - 5\n"
		"-\n"
		"  - hello, people\n"
		"  - goodbye\n"
		"  - mr.\n"
		"  - chips\n"
		"  - 99\n"
		"-";
}
