/* Author: macote */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN

#include "HashCheck.h"
#include "HashCheckWindow.h"
#include <string>
#include <vector>
#include <Windows.h>
#include <Ole2.h>
#include <CommCtrl.h>
#include <shellapi.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	int argscount;
	auto args = CommandLineToArgvW(GetCommandLineW(), &argscount);
	std::vector<std::wstring> argsvector;
	for (int i = 0; i < argscount; ++i)
	{
		argsvector.push_back(args[i]);
	}
	LocalFree(args);

	//if (SUCCEEDED(CoInitialize(NULL)))
	//{
	//	InitCommonControls();
	//	HashCheckWindow *hcw = HashCheckWindow::Create(hInstance);
	//	if (hcw)
	//	{
	//		ShowWindow(hcw->GetHWND(), nCmdShow);
	//		MSG msg;
	//		while (GetMessage(&msg, NULL, 0, 0))
	//		{
	//			TranslateMessage(&msg);
	//			DispatchMessage(&msg);
	//		}
	//	}
	//	CoUninitialize();
	//}

	HashCheck hc(argsvector);
	auto result = hc.Process();

	return result;
}
