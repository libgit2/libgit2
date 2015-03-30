/* Author: macote */

#include "Window.h"

void Window::Register()
{
	WNDCLASS wc;
	wc.style = 0;
	wc.lpfnWndProc = Window::WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hinst_;
	wc.hIcon = NULL;
	wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = ClassName();
	WinRegisterClass(&wc);
}

LRESULT CALLBACK Window::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	Window *self;
	if (uMsg == WM_NCCREATE)
	{
		auto lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
		self = reinterpret_cast<Window *>(lpcs->lpCreateParams);
		self->hwnd_ = hwnd;
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(self));
	}
	else
	{
		self = reinterpret_cast<Window *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	}
	if (self)
	{
		return self->HandleMessage(uMsg, wParam, lParam);
	}
	else
	{
		return DefWindowProcW(hwnd, uMsg, wParam, lParam);
	}
}

LRESULT Window::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LRESULT lres;
	switch (uMsg)
	{
	case WM_NCDESTROY:
		lres = DefWindowProcW(hwnd_, uMsg, wParam, lParam);
		SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
		delete this;
		return lres;
	case WM_PAINT:
		OnPaint();
		return 0;
	case WM_PRINTCLIENT:
		OnPrintClient(reinterpret_cast<HDC>(wParam));
		return 0;
	}
	return DefWindowProcW(hwnd_, uMsg, wParam, lParam);
}

void Window::OnPaint()
{
	PAINTSTRUCT ps;
	BeginPaint(hwnd_, &ps);
	PaintContent(&ps);
	EndPaint(hwnd_, &ps);
}

void Window::OnPrintClient(HDC hdc)
{
	PAINTSTRUCT ps;
	ps.hdc = hdc;
	GetClientRect(hwnd_, &ps.rcPaint);
	PaintContent(&ps);
}