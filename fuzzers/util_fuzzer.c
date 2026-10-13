/*
 * libgit2 utils fuzzer target.
 *
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "git2.h"
#include "date.h"
#include "net.h"
#include "signature.h"

#include "standalone_driver.h"

#define UNUSED(x) (void)(x)

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	UNUSED(argc);
	UNUSED(argv);

	if (git_libgit2_init() < 0)
		abort();

	return 0;
}

void fuzz_date(const uint8_t *data, size_t size) {
	git_time_t out;
	char *fuzz_cstr = NULL;

	fuzz_cstr = (char*)malloc(size+1);
	if (fuzz_cstr == NULL)
		return;
	memcpy(fuzz_cstr, data, size);
	fuzz_cstr[size] = '\0';

	git_date_parse(&out, fuzz_cstr);

	free(fuzz_cstr);
}

void fuzz_net(const uint8_t *data, size_t size) {
	git_net_url parsed_url, target;
	char *fuzz_cstr = (char*)malloc(size+1);

	memcpy(fuzz_cstr, data, size);
	fuzz_cstr[size] = '\0';

	memset(&parsed_url, 0, sizeof(parsed_url));
	if (git_net_url_parse_standard_or_scp(&parsed_url, fuzz_cstr) == 0) {
		git_net_url_matches_pattern(&parsed_url, "exa*mple.com*:443");
		git_net_url_apply_redirect(&parsed_url,
			"http://example.com/foo/bar/baz", false, "bar/baz");			
	}
	git_net_url_dispose(&parsed_url);

	memset(&parsed_url, 0, sizeof(parsed_url));
	if (git_net_url_parse_http(&parsed_url, fuzz_cstr) == 0) {
		memset(&target, 0, sizeof(target));
		git_net_url_dup(&target, &parsed_url);
		git_net_url_dispose(&target);
	}
	git_net_url_dispose(&parsed_url);

	memset(&parsed_url, 0, sizeof(parsed_url));
	if (git_net_url_parse_standard_or_scp(&parsed_url, fuzz_cstr) == 0) {
		memset(&target, 0, sizeof(target));
		git_net_url_joinpath(&target, &parsed_url, "/c/d");
		git_net_url_dispose(&target);
	};
	git_net_url_dispose(&parsed_url);

	free(fuzz_cstr);
}

void fuzz_signatures(const uint8_t *data, size_t size) {
	git_signature *signature = NULL;
	git_signature *signature2 = NULL;
	git_signature *signature3 = NULL;
	char *fuzz_cstr = NULL;

	fuzz_cstr = (char*)malloc(size+1);
	if (fuzz_cstr == NULL)
		return;
	memcpy(fuzz_cstr, data, size);
	fuzz_cstr[size] = '\0';

	git_signature_new(&signature,
		fuzz_cstr, fuzz_cstr, 1405694510, 0);
    
	
	git_signature_from_buffer(&signature2, fuzz_cstr);

	git_signature__equal(signature, signature2);

	if (git_signature_dup(&signature3, signature2) == 0)
		git_signature_free(signature3);

	git_signature_free(signature);
	git_signature_free(signature2);

	free(fuzz_cstr);
}

void fuzz_str(const uint8_t *data, size_t size) {
	git_str buf1 = GIT_STR_INIT;
	git_str buf2 = GIT_STR_INIT;
	git_str buf3 = GIT_STR_INIT;
	char *fuzz_cstr = NULL;

	fuzz_cstr = (char*)malloc(size+1);
	if (fuzz_cstr == NULL)
		return;
	memcpy(fuzz_cstr, data, size);
	fuzz_cstr[size] = '\0';

	git_str_puts(&buf1, fuzz_cstr);
	git_str_quote(&buf1);
	git_str_dispose(&buf1);

	git_str_decode_base85(&buf2, fuzz_cstr, size, 50);
	git_str_dispose(&buf2);

	git_str_puts_escaped(&buf3, fuzz_cstr, "asdf", "bd");
	git_str_dispose(&buf3);

	free(fuzz_cstr);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (size == 0)
		return 0;

	uint8_t decider = data[0];

	data++;
	size--;

	switch (decider % 4) {
		case 0: {
			fuzz_date(data, size);
			break;
		}
		case 1: {
			fuzz_net(data, size);
			break;
		}
		case 2: {
			fuzz_signatures(data, size);
			break;
		}
		case 3: {
			fuzz_str(data, size);
			break;
		}
	} 	
	return 0;
}
