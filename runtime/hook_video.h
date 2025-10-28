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
#ifndef HOOK_VIDEO_H_
#define HOOK_VIDEO_H_

#include "../common/kqf_win.h"

#ifdef __cplusplus
extern "C" {
#endif


extern
HWND video_window;


// KQF_CFGO_VIDEO_AVI, KQF_CFGO_CDROM_FAKE

void *__cdecl MSVCRT_fopen(char const *filename, char const *mode);

// KQF_CFGO_VIDEO_AVI, KQF_CFGO_VIDEO_NOBORDER

HWND VFWAPIV MSVFW32_MCIWndCreateA(HWND hwndParent, HINSTANCE hInstance, DWORD dwStyle, LPCSTR szFile);

// KQF_CFGO_VIDEO_NOAPPMOVE, KQF_CFGO_VIDEO_NOVIDMOVE

BOOL WINAPI USER32_MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint);


#ifdef __cplusplus
}
#endif
#endif
