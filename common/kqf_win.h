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
#ifndef KQF_WIN_H_
#define KQF_WIN_H_


#if defined(WINVER) || defined(_WIN32_WINDOWS) || defined(_WIN32_WINNT) || defined(_WIN32_IE)
# error Windows version already defined, include kqf_win.h instead of any Windows SDK headers.
#endif
#define WINVER         0x0400
#define _WIN32_WINDOWS 0x0400
#define _WIN32_IE      0x0300
#define _RICHEDIT_VER  0x0100
#define NOMINMAX
#define NONAMELESSUNION
#define WIN32_LEAN_AND_MEAN

#pragma warning(push, 1)
// patch_import() requires __declspec(dllimport) for hooked APIs
# define                     MCIWndCreateA extern_MCIWndCreateA
#  include <windows.h>
#  include <windowsx.h>
#  include <shlwapi.h>
#  include <richedit.h>
#  include <mmsystem.h>
#  include <vfw.h>
# undef                      MCIWndCreateA
DECLSPEC_IMPORT HWND VFWAPIV MCIWndCreateA(HWND hwndParent, HINSTANCE hInstance, DWORD dwStyle, LPCSTR szFile);
#pragma warning(pop)

#pragma warning(push, 1)
# include <intrin.h>
# pragma intrinsic(_InterlockedIncrement)
# define InterlockedIncrement _InterlockedIncrement
# pragma intrinsic(_InterlockedDecrement)
# define InterlockedDecrement _InterlockedDecrement
# pragma intrinsic(_InterlockedCompareExchange)
# define InterlockedCompareExchange _InterlockedCompareExchange
# pragma intrinsic(_ReturnAddress)
# define ReturnAddress _ReturnAddress()
# pragma intrinsic(__readfsdword)
# define TopLevelExceptionHandler __readfsdword(0)
#pragma warning(pop)

#pragma warning(push, 1)
# include <commctrl.h>
# include <objbase.h>
# include <shellapi.h>
# include <dbghelp.h>
#pragma warning(pop)

// _WIN32_WINNT >= 0x0500
#define ATTACH_PARENT_PROCESS ((DWORD)-1)
// _WIN32_WINNT >= 0x0501
#define CBM_FIRST 0x1700
#define CB_SETMINVISIBLE (CBM_FIRST + 1)
// _WIN32_IE >= 0x0501
typedef struct _DLLVERSIONINFO2_REMOVED {
	DLLVERSIONINFO info1;
	DWORD          dwFlags;
	ULONGLONG      ullVersion;
} DLLVERSIONINFO2_REMOVED;
#define DLLVER_MAJOR_MASK 0xFFFF000000000000
#define DLLVER_MINOR_MASK 0x0000FFFF00000000
#define DLLVER_BUILD_MASK 0x00000000FFFF0000
#define DLLVER_QFE_MASK   0x000000000000FFFF


#ifdef __cplusplus
extern "C" {
#endif


#define kqf_query_mem(addr, info) (VirtualQuery((addr), &(info), sizeof(MEMORY_BASIC_INFORMATION)) == sizeof(MEMORY_BASIC_INFORMATION))


#ifdef KQF_RUNTIME
extern int * (__cdecl *_imp___errno)(void);
#define MSVCRT_errno (*_imp___errno())
#endif


#ifdef __cplusplus
}
#endif
#endif