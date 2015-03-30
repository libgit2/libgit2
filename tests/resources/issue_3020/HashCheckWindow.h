/* Author: macote */

#ifndef HASHCHECKWINDOW_H_
#define HASHCHECKWINDOW_H_

#include "Window.h"
#include <Windows.h>
#include <windowsx.h>

class HashCheckWindow : public Window
{
public:
#if _MSC_VER < 1900
	HashCheckWindow(HINSTANCE hinst) : Window(hinst) { };
#else
	using Window::Window;
#endif	
	virtual LPCWSTR ClassName() { return L"HashCheckWindow"; }
	static HashCheckWindow *Create(HINSTANCE hInst);
protected:
	LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT OnCreate();
private:
	HWND hwndChild_;
};

#endif /* HASHCHECKWINDOW_H_ */