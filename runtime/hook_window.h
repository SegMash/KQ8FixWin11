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
#ifndef HOOK_WINDOW_H_
#define HOOK_WINDOW_H_

#include "../common/kqf_win.h"

#ifdef __cplusplus
extern "C" {
#endif


extern
HWND app_window;


BOOL WINAPI USER32_AdjustWindowRect(LPRECT lpRect, DWORD dwStyle, BOOL bMenu);
HWND WINAPI USER32_CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);

FARPROC WINAPI KERNEL32_GetProcAddress(HMODULE hModule, LPCSTR lpProcName);

BOOL WINAPI USER32_ClipCursor(CONST RECT *lpRect);
BOOL WINAPI USER32_GetCursorPos(LPPOINT lpPoint);
BOOL WINAPI USER32_SetCursorPos(int X, int Y);
int WINAPI USER32_ShowCursor(BOOL bShow);
BOOL WINAPI USER32_SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags);

HPALETTE WINAPI GDI32_CreatePalette(CONST LOGPALETTE *plpal);
HPALETTE WINAPI GDI32_SelectPalette(HDC hdc, HPALETTE hPal, BOOL bForceBkgd);
UINT WINAPI GDI32_RealizePalette(HDC hdc);
BOOL WINAPI GDI32_AnimatePalette(HPALETTE hPal, UINT iStartIndex, UINT cEntries, CONST PALETTEENTRY *ppe);
UINT WINAPI GDI32_GetSystemPaletteEntries(HDC hdc, UINT iStart, UINT cEntries, LPPALETTEENTRY pPalEntries);
HDC WINAPI USER32_GetDC(HWND hWnd);
int WINAPI USER32_ReleaseDC(HWND hWnd, HDC hDC);

#ifdef __cplusplus
}
#endif
#endif
