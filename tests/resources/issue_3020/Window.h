/* Author: macote */

#ifndef WINDOW_H_
#define WINDOW_H_

#include <Windows.h>

class Window
{
public:
	HWND GetHWND() const { return hwnd_; }
	Window(HINSTANCE hinst) : hinst_(hinst) { }
protected:
	virtual LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual void PaintContent(PAINTSTRUCT *pps) { }
	virtual LPCWSTR ClassName() = 0;
	virtual BOOL WinRegisterClass(WNDCLASS *pwc)
	{
		return RegisterClassW(pwc);
	}
	virtual ~Window() { }
	HWND WinCreateWindow(DWORD dwExStyle, LPCWSTR pszName, DWORD dwStyle, 
		int x, int y, int cx, int cy, HWND hwndParent, HMENU hmenu)
	{
		Register();
		return CreateWindowExW(dwExStyle, ClassName(), pszName, dwStyle,
			x, y, cx, cy, hwndParent, hmenu, hinst_, this);
	}
	HWND hwnd_;
	HINSTANCE hinst_;
private:
	void Register();
	void OnPaint();
	void OnPrintClient(HDC hdc);
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

#endif /* WINDOW_H_ */