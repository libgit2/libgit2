/*
 * Copyright (C) the libgit2 contributors. All rights reserved.
 *
 * This file is part of libgit2, distributed under the GNU GPL v2 with
 * a Linking Exception. For full terms see the included COPYING file.
 */

#include "auth_default.h"

#ifdef GIT_WIN32

#include "git2/sys/credential.h"

/* For IInternetSecurityManager zone check */
#include <objbase.h>
#include <urlmon.h>

int git_http_auth_default_fallback(
	git_credential **cred,
	const char *url,
	unsigned int allowed_types)
{
	IInternetSecurityManager *pISM;
	wchar_t *wide_url;
	HRESULT hCoInitResult;
	DWORD dwZone;
	int error = 1;

	/* If the target URI supports integrated Windows authentication */
	if (!(allowed_types & GIT_CREDENTIAL_DEFAULT))
		return 1;

	if (git_utf8_to_16_alloc(&wide_url, url) < 0) {
		git_error_set(GIT_ERROR_OS, "failed to convert string to wide form");
		return -1;
	}

	hCoInitResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);

	/* And if the target URI is in the My Computer, Intranet, or Trusted zones */
	if ((SUCCEEDED(hCoInitResult) || hCoInitResult == RPC_E_CHANGED_MODE) &&
	    SUCCEEDED(CoCreateInstance(&CLSID_InternetSecurityManager, NULL,
		CLSCTX_ALL, &IID_IInternetSecurityManager, (void **)&pISM))) {
		if (SUCCEEDED(pISM->lpVtbl->MapUrlToZone(pISM, wide_url, &dwZone, 0)) &&
		    (URLZONE_LOCAL_MACHINE == dwZone ||
		     URLZONE_INTRANET == dwZone ||
		     URLZONE_TRUSTED == dwZone)) {
			git_credential_free(*cred);

			/* Then use default Windows credentials to authenticate this request */
			error = git_credential_default_new(cred);
		}

		pISM->lpVtbl->Release(pISM);
	}

	/* Only uninitialize if the call to CoInitializeEx was successful. */
	if (SUCCEEDED(hCoInitResult))
		CoUninitialize();

	git__free(wide_url);
	return error;
}

#endif
