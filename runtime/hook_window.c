/*
 * Copyright (c) 2014,2016,2019 Nico Bendlin <nico@nicode.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "hook_window.h"

#include "runtime.rh"
#include "../common/kqf_app.h"
#include "../common/kqf_cfg.h"
#include "../common/kqf_log.h"


HWND app_window /* = NULL */;


////////////////////////////////////////////////////////////////////////////////
// detect 3Dfx open/close and avoid recursion

static LONG /*volatile*/ glide_inopen /* = FALSE */;
static LONG /*volatile*/ glide_active /* = FALSE */;

static int activate_app_window(void)
{
	if (!app_window) {
		kqf_log(KQF_LOGL_WARNING, "Glide: app window is not detected\n");
		return (0);
	}
	if (GetActiveWindow() != app_window) {
		kqf_log(KQF_LOGL_INFO, "Glide: app window is not active\n");
		if (!SetActiveWindow(app_window)) {
			if (!IsWindowVisible(app_window)) {
				kqf_log(KQF_LOGL_INFO, "Glide: app window is invisible\n");
				ShowWindow(app_window, SW_SHOW);
			}
			if (IsIconic(app_window)) {
				kqf_log(KQF_LOGL_INFO, "Glide: app window is minimized\n");
				ShowWindow(app_window, SW_RESTORE);
			}
			if (!SetActiveWindow(app_window)) {
				kqf_log(KQF_LOGL_WARNING, "Glide: app window is still inactive\n");
				return (0);
			}
		}
	}
	return (1);
}

static int (__stdcall *glide2x_grSstWinOpen)(unsigned long hWnd, signed long screen_resolution, signed long refresh_rate, signed long color_format, signed long origin_location, int nColBuffers, int nAuxBuffers) /* = NULL */;
static int  __stdcall  GLIDE2X_grSstWinOpen (unsigned long hWnd, signed long screen_resolution, signed long refresh_rate, signed long color_format, signed long origin_location, int nColBuffers, int nAuxBuffers)
{
	int result;
	KQF_TRACE("grSstWinOpen<%#08lx>(%#08lx,%lu,%li,%li,%li,%i,%i)\n", ReturnAddress, (HWND)hWnd, screen_resolution, refresh_rate, color_format, origin_location, nColBuffers, nAuxBuffers);
	if (InterlockedCompareExchange(&glide_inopen, TRUE, FALSE)) {
		kqf_log(KQF_LOGL_WARNING, "Glide: open already in progress\n");
		result = 0;
	} else {
		kqf_log(KQF_LOGL_DEBUG, "Glide: open (%li)\n", screen_resolution);
		if (!activate_app_window() && !hWnd) {
			hWnd = (unsigned long)app_window;
			kqf_log(KQF_LOGL_WARNING, "Glide: failed to activate app window\n");
		}
		{
			int retries = 0;
			while ((result = glide2x_grSstWinOpen(hWnd, screen_resolution, refresh_rate, color_format, origin_location, nColBuffers, nAuxBuffers), !result) && (retries++ < 5)) {
				kqf_log(KQF_LOGL_WARNING, "Glide: retry grSstWinOpen(%#08lx,%lu,...) #%i\n", (HWND)hWnd, screen_resolution, retries);
				activate_app_window();
			}
		}
		if (result) {
			InterlockedCompareExchange(&glide_active, TRUE, FALSE);
		}
		InterlockedCompareExchange(&glide_inopen, FALSE, TRUE);
		kqf_log(KQF_LOGL_DEBUG, "Glide: open (%li)[%i]\n", screen_resolution, result);
	}
	KQF_TRACE("grSstWinOpen<%#08lx>(%#08lx,%lu,%li,%li,%li,%i,%i)[%i]\n", ReturnAddress, (HWND)hWnd, screen_resolution, refresh_rate, color_format, origin_location, nColBuffers, nAuxBuffers, result);
	return (result);
}

static void (__stdcall *glide2x_grSstWinClose)(void) /* = NULL */;
static void  __stdcall  GLIDE2X_grSstWinClose (void)
{
	KQF_TRACE("grSstWinClose<%#08lx>()\n", ReturnAddress);
	if (InterlockedCompareExchange(&glide_inopen, TRUE, TRUE)) {
		kqf_log(KQF_LOGL_WARNING, "Glide: close during open\n");
	} else {
		kqf_log(KQF_LOGL_DEBUG, "Glide: close()\n");
		glide2x_grSstWinClose();
		kqf_log(KQF_LOGL_DEBUG, "Glide: close()[]\n");
		InterlockedCompareExchange(&glide_active, FALSE, TRUE);
	}
	KQF_TRACE("grSstWinClose<%#08lx>()[]\n", ReturnAddress);
}


////////////////////////////////////////////////////////////////////////////////
// handle main window messages

static LONG /*volatile*/ wm_size_level /* = 0 */;

static LRESULT (CALLBACK *mask_GWWindow_WndProc)(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) /* = NULL */;
static LRESULT  CALLBACK  MASK_GWWindow_WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LRESULT result;
	BOOL CallDefProc = (NULL == mask_GWWindow_WndProc);
	//HACK: KQF_TRACE_WINDOW("WndProc<%#08lx>(%#04x,%#08lx,%#08lx)\n", hWnd, uMsg, wParam, lParam);
	if (glide_inopen) {
		kqf_log(KQF_LOGL_DEBUG, "WndProc<%#08lx>(%#04x,%#08lx,%#08lx) in grSstWinOpen \n", hWnd, uMsg, wParam, lParam);
		//HACK: CallDefProc = TRUE;
	}
	switch (uMsg) {
	case WM_DESTROY:
		kqf_log(KQF_LOGL_DEBUG, "WndProc<%#08lx>: WM_DESTROY\n", hWnd);
		break;
	case WM_SIZE:
		kqf_log(KQF_LOGL_DEBUG, "WndProc<%#08lx>: WM_SIZE(%u,%hux%hu)\n", hWnd, wParam, LOWORD(lParam), HIWORD(lParam));
		if (wm_size_level) {
			kqf_log(KQF_LOGL_WARNING, "WndProc<%#08lx>: recursive WM_SIZE (%d)\n", hWnd, wm_size_level);
		}
		if (kqf_get_opt(KQF_CFGO_GLIDE_NOWMSIZE) && glide_active) {
			kqf_log(KQF_LOGL_INFO, "WndProc<%#08lx>: ignore WM_SIZE for Glide\n", hWnd);
			CallDefProc = TRUE;
		}
		if (kqf_get_opt(KQF_CFGO_WINDOW_NOBORDER)) {
			/* ignore WM_SIZE if it matches the screen resolution */
			if ((LOWORD(lParam) == GetSystemMetrics(SM_CXSCREEN)) &&
			    (HIWORD(lParam) == GetSystemMetrics(SM_CYSCREEN))) {
				kqf_log(KQF_LOGL_INFO, "WndProc<%#08lx>: ignore WM_SIZE in fullscreen (%hux%hu)\n", hWnd, LOWORD(lParam), HIWORD(lParam));
				CallDefProc = TRUE;
			}
		}
		/* ignore WM_SIZE notifications for other windows */
		if ((hWnd != app_window) || (wParam != SIZE_RESTORED)) {
			kqf_log(KQF_LOGL_INFO, "WndProc<%#08lx>: ignore WM_SIZE notification (%u)\n", hWnd, wParam);
			CallDefProc = TRUE;
		}
		break;
	case WM_SETFOCUS:
		kqf_log(KQF_LOGL_DEBUG, "WndProc<%#08lx>: WM_SETFOCUS(%#08lx)\n", hWnd, wParam);
		break;
	case WM_KILLFOCUS:
		kqf_log(KQF_LOGL_DEBUG, "WndProc<%#08lx>: WM_KILLFOCUS(%#08lx)\n", hWnd, wParam);
		//FIXME: Windows 10 focus-loss on video playback...
		break;
	case WM_PAINT:
		kqf_log(KQF_LOGL_DEBUG, "WndProc<%#08lx>: WM_PAINT\n", hWnd);
		break;
	case WM_ERASEBKGND:
		kqf_log(KQF_LOGL_DEBUG, "WndProc<%#08lx>: WM_ERASEBKGND(%#08lx)\n", hWnd, wParam);
		break;
	case WM_SHOWWINDOW:
		kqf_log(KQF_LOGL_DEBUG, "WndProc<%#08lx>: WM_SHOWWINDOW(%u,%lu)\n", hWnd, wParam, lParam);
		break;
	case WM_ACTIVATEAPP:
		kqf_log(KQF_LOGL_DEBUG, "WndProc<%#08lx>: WM_ACTIVATEAPP(%u,%li)\n", hWnd, wParam, lParam);
		break;
	case WM_GETMINMAXINFO:
		kqf_log(KQF_LOGL_DEBUG, "WndProc<%#08lx>: WM_GETMINMAXINFO\n", hWnd);
		break;
	case WM_NCDESTROY:
		kqf_log(KQF_LOGL_DEBUG, "WndProc<%#08lx>: WM_NCDESTROY\n", hWnd);
		break;
	case WM_KEYDOWN:
	case WM_KEYUP:
		switch (wParam) {
		case VK_SNAPSHOT:
			kqf_log(KQF_LOGL_DEBUG, "WndProc<%#08lx>: VK_SNAPSHOT (%i,%#08lx)\n", hWnd, uMsg - WM_KEYDOWN, lParam);
			break;
		case VK_LWIN:
		case VK_RWIN:
			kqf_log(KQF_LOGL_INFO, "WndProc<%#08lx>: ignore OS key (%i,%#08lx)\n", hWnd, uMsg - WM_KEYDOWN, lParam);
			CallDefProc = TRUE;
		}
		break;
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
		switch (wParam) {
		case VK_TAB:
			kqf_log(KQF_LOGL_INFO, "WndProc<%#08lx>: ignore Alt+Tab (%i,%#08lx)\n", hWnd, uMsg - WM_SYSKEYDOWN, lParam);
			CallDefProc = TRUE;
		case VK_RETURN:
			kqf_log(KQF_LOGL_DEBUG, "WndProc<%#08lx>: Alt+Enter (%i,%#08lx)\n", hWnd, uMsg - WM_SYSKEYDOWN, lParam);
			break;
		case VK_MENU:
			kqf_log(KQF_LOGL_DEBUG, "WndProc<%#08lx>: Alt+Menu (%i,%#08lx)\n", hWnd, uMsg - WM_SYSKEYDOWN, lParam);
			break;
		case VK_ESCAPE:
			kqf_log(KQF_LOGL_DEBUG, "WndProc<%#08lx>: Alt+Escape (%i,%#08lx)\n", hWnd, uMsg - WM_SYSKEYDOWN, lParam);
			break;
		}
		break;
	case WM_SYSCOMMAND:
		kqf_log(KQF_LOGL_DEBUG, "WndProc<%#08lx>: WM_SYSCOMMAND(%u,%i,%i)\n", hWnd, wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
	case WM_QUERYNEWPALETTE:
		kqf_log(KQF_LOGL_DEBUG, "WndProc<%#08lx>: WM_QUERYNEWPALETTE\n", hWnd);
		break;
	case WM_PALETTECHANGED:
		kqf_log(KQF_LOGL_DEBUG, "WndProc<%#08lx>: WM_PALETTECHANGED(%#08lx)\n", hWnd, wParam);
		if ((hWnd != (HWND)wParam) && (hWnd == GetActiveWindow())) {
			kqf_log(KQF_LOGL_INFO, "Palette: another window changed the palette while the game is active (%#08lx)\n", wParam);
			return (TRUE);
		}
		break;
	case WM_PALETTEISCHANGING:
		kqf_log(KQF_LOGL_DEBUG, "WndProc<%#08lx>: WM_PALETTEISCHANGING(%#08lx)\n", hWnd, wParam);
		break;
#ifdef KQF_DEBUG
	default:
		if ((0xC000 <= uMsg) && (uMsg <= 0xFFFF)) {
			CHAR Name[256];
			if (GetAtomNameA((ATOM)uMsg, Name, ARRAYSIZE(Name))) {
				kqf_log(KQF_LOGL_DEBUG, "WndProc<%#08lx>: la'%s' (%#04x,%#08lx,%#08lx)\n", hWnd, Name, uMsg, wParam, lParam);
			} else if (GlobalGetAtomNameA((ATOM)uMsg, Name, ARRAYSIZE(Name))) {
				kqf_log(KQF_LOGL_DEBUG, "WndProc<%#08lx>: ga'%s' (%#04x,%#08lx,%#08lx)\n", hWnd, Name, uMsg, wParam, lParam);
			} else if (GetClipboardFormatNameA(uMsg, Name, ARRAYSIZE(Name))) {
				kqf_log(KQF_LOGL_DEBUG, "WndProc<%#08lx>: rm'%s' (%#04x,%#08lx,%#08lx)\n", hWnd, Name, uMsg, wParam, lParam);
			}
		}
		break;
#endif
	}
	if (WM_SIZE == uMsg) {
		InterlockedIncrement(&wm_size_level);
	}
	result = CallDefProc
		? DefWindowProcA(hWnd, uMsg, wParam, lParam)
		: CallWindowProcA(mask_GWWindow_WndProc, hWnd, uMsg, wParam, lParam);
	if (WM_SIZE == uMsg) {
		InterlockedDecrement(&wm_size_level);
	}
	//HACK: KQF_TRACE_WINDOW("WndProc<%#08lx>(%#04x,%#08lx,%#08lx)[%#08lx]\n", hWnd, uMsg, wParam, lParam, result);
	return (result);
}


BOOL WINAPI USER32_AdjustWindowRect(LPRECT lpRect, DWORD dwStyle, BOOL bMenu)
{
	BOOL result;
	KQF_TRACE("AdjustWindowRect<%#08lx>(<%li,%li,%li,%li>,%#lx,%i)\n", ReturnAddress, lpRect ? lpRect->left : 0L, lpRect ? lpRect->top : 0L, lpRect ? lpRect->right : 0L, lpRect ? lpRect->bottom : 0L, dwStyle, bMenu);
	if (kqf_get_opt(KQF_CFGO_WINDOW_NOBORDER) && ((WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN) == dwStyle)) {
		kqf_log(KQF_LOGL_INFO, "AdjustWindowRect: overriding window style\n");
		dwStyle = WS_POPUP | WS_CLIPCHILDREN;
	}
	result = AdjustWindowRect(lpRect, dwStyle, bMenu);
	KQF_TRACE("AdjustWindowRect<%#08lx>(<%li,%li,%li,%li>,%#lx,%i)[%i]{%#lx}\n", ReturnAddress, lpRect ? lpRect->left : 0L, lpRect ? lpRect->top : 0L, lpRect ? lpRect->right : 0L, lpRect ? lpRect->bottom : 0L, dwStyle, bMenu, result, result ? ERROR_SUCCESS : GetLastError());
	return (result);
}

HWND WINAPI USER32_CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
	HWND result;
	CHAR title[64];
	KQF_TRACE("CreateWindowExA<%#08lx>(%#08lx,%#lx,%#lx,%i,%i,%i,%i,'%s','%s')\n", ReturnAddress, hWndParent, dwStyle, dwExStyle, X, Y, nWidth, nHeight, lpClassName, lpWindowName);
	if (kqf_get_opt(KQF_CFGO_WINDOW_TITLE) &&
	    (!lpWindowName || ('\0' == *lpWindowName) || (0 == lstrcmpiA(lpWindowName, "Window")))) {
		if (LoadStringA(kqf_mod, IDS_APP_WINDOW_TITLE, title, ARRAYSIZE(title)) > 0) {
			kqf_log(KQF_LOGL_INFO, "CreateWindowExA: overriding window title\n");
			lpWindowName = title;
		}
	}
	if (kqf_get_opt(KQF_CFGO_WINDOW_NOBORDER) && ((WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN) == dwStyle)) {
		kqf_log(KQF_LOGL_INFO, "CreateWindowExA: overriding window styles\n");
		dwStyle = WS_POPUP | WS_CLIPCHILDREN;
		dwExStyle = WS_EX_APPWINDOW;
	}
	result = CreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
	if ((result != NULL) && ((WS_CLIPCHILDREN & dwStyle) != 0)) {
		WNDPROC proc_get = (WNDPROC)(LONG_PTR)(SetLastError(ERROR_SUCCESS), GetWindowLongA(result, GWL_WNDPROC));
		if (!proc_get && (GetLastError() != ERROR_SUCCESS)) {
			kqf_log(KQF_LOGL_ERROR, "CreateWindowExA: failed to retrieve the window procedure [%#08lx]{%#lx}\n", result, GetLastError());
		} else if (InterlockedCompareExchangePointer(&mask_GWWindow_WndProc, proc_get, NULL)) {
			kqf_log(KQF_LOGL_WARNING, "CreateWindowExA: window procedure hook already exists\n");
		} else {
			WNDPROC proc_set = (WNDPROC)(LONG_PTR)(SetLastError(ERROR_SUCCESS), SetWindowLongA(result, GWL_WNDPROC, (LONG)(LONG_PTR)MASK_GWWindow_WndProc));
			if (!proc_set && (GetLastError() != ERROR_SUCCESS)) {
				kqf_log(KQF_LOGL_ERROR, "CreateWindowExA: failed to override the window procedure [%#08lx]{%#lx}\n", result, GetLastError());
			} else if (proc_get != proc_set) {
				kqf_log(KQF_LOGL_WARNING, "CreateWindowExA: window procedure get/set mismatch (%#08lx,%#08lx)\n", proc_get, proc_set);
				InterlockedCompareExchangePointer(&mask_GWWindow_WndProc, proc_set, proc_get);
			} else {
				kqf_log(KQF_LOGL_INFO, "CreateWindowExA: window procedure redirected (%#08lx -> %#08lx)\n", mask_GWWindow_WndProc, &MASK_GWWindow_WndProc);
			}
		}
		kqf_log(KQF_LOGL_INFO, "CreateWindowExA: app window detected (%#08lx)\n", result);
		app_window = result;
	}
	KQF_TRACE("CreateWindowExA<%#08lx>(%#08lx,%#lx,%#lx,%i,%i,%i,%i,'%s','%s')[%#08lx]{%#lx}\n", ReturnAddress, hWndParent, dwStyle, dwExStyle, X, Y, nWidth, nHeight, lpClassName, lpWindowName, result, (result != NULL) ? ERROR_SUCCESS : GetLastError());
	return result;
}


FARPROC WINAPI KERNEL32_GetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
	FARPROC result = GetProcAddress(hModule, lpProcName);
	if ((result != NULL) && ((ULONG_PTR)lpProcName != (ULONG_PTR)LOWORD(lpProcName))) {
		if (0 == lstrcmpA(lpProcName, "_grSstWinOpen@28")) {
			glide2x_grSstWinOpen = (int (__stdcall *)(unsigned long, signed long, signed long, signed long, signed long, int, int))result;
			result = (FARPROC)GLIDE2X_grSstWinOpen;
		} else if (0 == lstrcmpA(lpProcName, "_grSstWinClose@0")) {
			glide2x_grSstWinClose = (void (__stdcall *)(void))result;
			result = (FARPROC)GLIDE2X_grSstWinClose;
		}
	}
	return (result);
}


BOOL WINAPI USER32_ClipCursor(CONST RECT *lpRect)
{
	BOOL result;
	result = ClipCursor(lpRect);
	if (NULL == lpRect) {
		kqf_log(KQF_LOGL_DEBUG, "ClipCursor<%#08lx>(NULL)[%i]\n", ReturnAddress, result);
	} else {
		kqf_log(KQF_LOGL_DEBUG, "ClipCursor<%#08lx>(%li,%li,%li,%li)[%i]\n", ReturnAddress, lpRect->left, lpRect->top, lpRect->right, lpRect->bottom, result);
		if (result) {
			RECT Clip;
			DWORD ErrCode = GetLastError();
			if (GetClipCursor(&Clip)) {
				kqf_log(KQF_LOGL_DEBUG, "ClipCursor: effective: %li,%li,%li,%li\n", Clip.left, Clip.top, Clip.right, Clip.bottom);
			}
			SetLastError(ErrCode);
		}
	}
	return (result);
}


BOOL WINAPI USER32_GetCursorPos(LPPOINT lpPoint)
{
	BOOL result = GetCursorPos(lpPoint);
	kqf_log(KQF_LOGL_DEBUG, "GetCursorPos<%#08lx>(%i,%i)[%i]{%#lx}\n", ReturnAddress, lpPoint ? lpPoint->x : -1, lpPoint ? lpPoint->y : -1, result, result ? ERROR_SUCCESS : GetLastError());
	return (result);
}

BOOL WINAPI USER32_SetCursorPos(int X, int Y)
{
	BOOL result = SetCursorPos(X, Y);
	kqf_log(KQF_LOGL_DEBUG, "SetCursorPos<%#08lx>(%i,%i)[%i]{%#lx}\n", ReturnAddress, X, Y, result, result ? ERROR_SUCCESS : GetLastError());
	if (result) {
		POINT Pos;
		DWORD ErrCode = GetLastError();
		if (GetCursorPos(&Pos)) {
			kqf_log(KQF_LOGL_DEBUG, "SetCursorPos: effective: %i,%i\n", Pos.x, Pos.y);
		}
		SetLastError(ErrCode);
	}
	return (result);
}


int WINAPI USER32_ShowCursor(BOOL bShow)
{
	int result = ShowCursor(bShow);
	kqf_log(KQF_LOGL_DEBUG, "ShowCursor<%#08lx>(%i)[%i]\n", ReturnAddress, bShow, result);
	return (result);
}

BOOL WINAPI USER32_SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
{
	BOOL result;
	KQF_TRACE("SetWindowPos<%#08lx>(%#08lx,%#08lx,%i,%i,%i,%i,0x%08X)\n", ReturnAddress, hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
	if (hWnd == app_window) {
		switch(uFlags) {
		case (0):
			if (HWND_TOP == hWndInsertAfter) {
				kqf_log(KQF_LOGL_DEBUG, "SetWindowPos: GFXDevice::setWindow(%i,%i,%i,%i)\n", X, Y, cx, cy);
			} else {  /* HWND_TOPMOST */
				kqf_log(KQF_LOGL_DEBUG, "SetWindowPos: GFXDevice::RestoreWindow(%i,%i,%i,%i)\n", X, Y, cx, cy);
			}
			break;
		case (SWP_NOSIZE | SWP_NOMOVE):
			/* Called for every WM_SHOWWINDOW before beeing passed to the default handler. */
			kqf_log(KQF_LOGL_DEBUG, "SetWindowPos: GWCanvas::onShowWindow()\n");
			break;
		case (SWP_NOSIZE | SWP_NOZORDER):
			kqf_log(KQF_LOGL_DEBUG, "SetWindowPos: GWWindow::setPosition(%i,%i)\n", X, Y);
			break;
		case (SWP_NOMOVE | SWP_NOZORDER):
			kqf_log(KQF_LOGL_DEBUG, "SetWindowPos: GWWindow::setSize(%i,%i)\n", cx, cy);
			break;
		case (SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE):
			/* only happens before switching Glide resolution (800x600 on/off) */
			kqf_log(KQF_LOGL_DEBUG, "SetWindowPos: GWWindow::setClientSize(%i,%i)\n", cx, cy);
			break;
		default:
			/* unexpected usage */
			kqf_log(KQF_LOGL_DEBUG, "SetWindowPos<%#08lx>(%#08lx,%i,%i,%i,%i,0x%08X)\n", ReturnAddress, hWndInsertAfter, X, Y, cx, cy, uFlags);
			break;
		}
		if (HWND_TOPMOST == hWndInsertAfter) {
			//HACK: no top-most window
			uFlags |= SWP_NOZORDER | SWP_NOOWNERZORDER;
		}
	}

	result = SetWindowPos(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
	KQF_TRACE("SetWindowPos<%#08lx>(%#08lx,%#08lx,%i,%i,%i,%i,0x%08X)[%i]{%#lx}\n", ReturnAddress, hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags, result, result ? ERROR_SUCCESS : GetLastError());
	return (result);
}


HPALETTE WINAPI GDI32_CreatePalette(CONST LOGPALETTE *plpal)
{
	HPALETTE result;
	kqf_log(KQF_LOGL_DEBUG, "CreatePalette<%#08lx>(%i,%i)\n", ReturnAddress, plpal ? plpal->palVersion : 0, plpal ? plpal->palNumEntries : 0);
	result = CreatePalette(plpal);
	kqf_log(KQF_LOGL_DEBUG, "CreatePalette<%#08lx>(%i,%i)[%#08lx]{%#lx}\n", ReturnAddress, plpal ? plpal->palVersion : 0, plpal ? plpal->palNumEntries : 0, result, result ? ERROR_SUCCESS : GetLastError());
	return (result);
}

HPALETTE WINAPI GDI32_SelectPalette(HDC hdc, HPALETTE hPal, BOOL bForceBkgd)
{
	HPALETTE result;
	kqf_log(KQF_LOGL_DEBUG, "SelectPalette<%#08lx>(%#08lx[%#08lx],%#08lx,%i)\n", ReturnAddress, hdc, WindowFromDC(hdc), hPal, bForceBkgd);
	result = (hPal == (HPALETTE)0x0188000b) ? NULL : SelectPalette(hdc, hPal, bForceBkgd);
	kqf_log(KQF_LOGL_DEBUG, "SelectPalette<%#08lx>(%#08lx[%#08lx],%#08lx,%i)[%#08lx]{%#lx}\n", ReturnAddress, hdc, WindowFromDC(hdc), hPal, bForceBkgd, result, result ? ERROR_SUCCESS : GetLastError());
	return (result);
}

UINT WINAPI GDI32_RealizePalette(HDC hdc)
{
	UINT result;
	kqf_log(KQF_LOGL_DEBUG, "RealizePalette<%#08lx>(%#08lx[%#08lx])\n", ReturnAddress, hdc, WindowFromDC(hdc));
	result = RealizePalette(hdc);
	kqf_log(KQF_LOGL_DEBUG, "RealizePalette<%#08lx>(%#08lx[%#08lx])[%i]{%#lx}\n", ReturnAddress, hdc, WindowFromDC(hdc), result, (result != GDI_ERROR) ? ERROR_SUCCESS : GetLastError());
	return (result);
}

BOOL WINAPI GDI32_AnimatePalette(HPALETTE hPal, UINT iStartIndex, UINT cEntries, CONST PALETTEENTRY *ppe)
{
	BOOL result;
	kqf_log(KQF_LOGL_DEBUG, "AnimatePalette<%#08lx>(%#08lx,%i,%i,%#08lx)\n", ReturnAddress, hPal, iStartIndex, cEntries, ppe);
	result = AnimatePalette(hPal, iStartIndex, cEntries, ppe);
	kqf_log(KQF_LOGL_DEBUG, "AnimatePalette<%#08lx>(%#08lx,%i,%i,%#08lx)[%i]{%#lx}\n", ReturnAddress, hPal, iStartIndex, cEntries, ppe, result, result ? ERROR_SUCCESS : GetLastError());
	return (result);
}

UINT WINAPI GDI32_GetSystemPaletteEntries(HDC hdc, UINT iStart, UINT cEntries, LPPALETTEENTRY pPalEntries)
{
	UINT result;
	kqf_log(KQF_LOGL_DEBUG, "GetSystemPaletteEntries<%#08lx>(%#08lx[%#08lx],%i,%i,%#08lx)\n", ReturnAddress, hdc, WindowFromDC(hdc), iStart, cEntries, pPalEntries);
	result = GetSystemPaletteEntries(hdc, iStart, cEntries, pPalEntries);
	kqf_log(KQF_LOGL_DEBUG, "GetSystemPaletteEntries<%#08lx>(%#08lx[%#08lx],%i,%i,%#08lx)[%i]{%#lx}\n", ReturnAddress, hdc, WindowFromDC(hdc), iStart, cEntries, pPalEntries, result, result ? ERROR_SUCCESS : GetLastError());
	return (result);
}

HDC WINAPI USER32_GetDC(HWND hWnd)
{
	HDC result;
	kqf_log(KQF_LOGL_DEBUG, "GetDC<%#08lx>(%#08lx)\n", ReturnAddress, hWnd);
	result = GetDC(hWnd);
	kqf_log(KQF_LOGL_DEBUG, "GetDC<%#08lx>(%#08lx)[%#08lx]{%#lx}\n", ReturnAddress, hWnd, result, result ? ERROR_SUCCESS : GetLastError());
	return (result);
}

int WINAPI USER32_ReleaseDC(HWND hWnd, HDC hDC)
{
	int result;
	kqf_log(KQF_LOGL_DEBUG, "ReleaseDC<%#08lx>(%#08lx,%#08lx)\n", ReturnAddress, hWnd, hDC);
	result = ReleaseDC(hWnd, hDC);
	kqf_log(KQF_LOGL_DEBUG, "ReleaseDC<%#08lx>(%#08lx,%#08lx)[%i]{%#lx}\n", ReturnAddress, hWnd, hDC, result, result ? ERROR_SUCCESS : GetLastError());
	return (result);
}