/* Author: macote */

#include "HashCheckWindow.h"

LRESULT HashCheckWindow::OnCreate()
{
	return 0;
}

LRESULT HashCheckWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
		return OnCreate();
	case WM_NCDESTROY:
		// Death of the root window ends the thread
		PostQuitMessage(0);
		break;
	case WM_SIZE:
		if (hwndChild_)
		{
			SetWindowPos(hwndChild_, NULL, 0, 0, GET_X_LPARAM(lParam), 
				GET_Y_LPARAM(lParam), SWP_NOZORDER | SWP_NOACTIVATE);
		}
		return 0;
	case WM_SETFOCUS:
		if (hwndChild_)
		{
			SetFocus(hwndChild_);
		}
		return 0;
	}
	return Window::HandleMessage(uMsg, wParam, lParam);
}

HashCheckWindow* HashCheckWindow::Create(HINSTANCE hInst)
{
	auto self = new HashCheckWindow(hInst);
	if (self != NULL)
	{
		if (self->WinCreateWindow(0, L"HashCheckWindow", WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL))
		{
			return self;
		}
		delete self;
	}
	return NULL;
}
