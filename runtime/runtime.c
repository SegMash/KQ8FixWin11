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
#include "runtime.h"
#include "runtime.rh"

#include "../common/kqf_app.h"
#include "../common/kqf_cfg.h"
#include "../common/kqf_log.h"
#include "../common/kqf_init.h"
#include "../common/kqf_win.h"

#include "hook_shim.h"
#include "hook_talk.h"
#include "hook_video.h"
#include "hook_cdrom.h"
#include "hook_window.h"
#include "hook_memory.h"
#include "hook_gfx.h"

////////////////////////////////////////////////////////////////////////////////
//
//                 Allow to query the module version at runtime
//
//  Note that info1.dwBuildNumber and ullVersion & DLLVER_QFE_MASK contain the
//  build type (VS_FF_DEBUG, VS_FF_PRIVATEBUILD) and not a consecutive number.
//

HRESULT CALLBACK RUNTIME_DllGetVersion(DLLVERSIONINFO2 *pdvi)
{
	if ((NULL == pdvi) || (pdvi->info1.cbSize < sizeof(DLLVERSIONINFO))) {
		return (E_INVALIDARG);
	}
	pdvi->info1.dwMajorVersion = KQF_VERF_MAJOR;
	pdvi->info1.dwMinorVersion = KQF_VERF_MINOR;
	pdvi->info1.dwBuildNumber  = KQF_VERF_FLAGS;
	pdvi->info1.dwPlatformID   = DLLVER_PLATFORM_WINDOWS;
	if (sizeof(DLLVERSIONINFO2) <= pdvi->info1.cbSize) {
		pdvi->dwFlags    = 0;
		pdvi->ullVersion = MAKEDLLVERULL(KQF_VERF_MAJOR, KQF_VERF_MINOR, KQF_VERF_PATCH, KQF_VERF_FLAGS);
	}
	return (S_OK);
}


////////////////////////////////////////////////////////////////////////////////
//
//  Manually wrap MSVCRT._onexit by calling the imported function pointer.
//  (the MSVCRT.LIB '_onexit' symbol refers to code instead of the import)
//

extern
int (*(__cdecl *_imp___onexit)(int (__cdecl *_Func)(void)))(void);
int (* __cdecl MSVCRT__onexit (int (__cdecl *_Func)(void)))(void)
{
	return (_imp___onexit(_Func));
}


////////////////////////////////////////////////////////////////////////////////
//
//                     Disable and/or log game debug output
//
//  By default the game debug ouput is disabled to avoid the API performance
//  overhead and to get rid of the output in a debugger/monitor (most of the
//  output is written to the game's own log if debug == true in Console.cs).
//  However, for research/debugging purposes it might be useful to write the
//  game debug output into the runtime log (independent from the log level).
//

static VOID WINAPI KERNEL32_OutputDebugStringA(LPCSTR lpOutputString)
{
	KQF_OPT_MASK_DBG_ opt = kqf_get_opt(KQF_CFGO_MASK_DBG);
	if ((KQF_OPT_MASK_DBG_LOG & opt) != 0) {
		int const length = lpOutputString ? lstrlenA(lpOutputString) : 0;
		if (length > 0) {
			char const *format = (length > 1005) ? "[mask.log] %.1005s [...]\n" : (
				(lpOutputString[length - 1] != '\n') ? "[mask.log] %s\n" : "[mask.log] %s");
			kqf_log(KQF_LOGL_FORCE, format, lpOutputString);
			// do not output twice if logging with ODS
			if ((KQF_LOGT_ODS & kqf_get_log_type()) != 0) {
				return;
			}
		}
	}
	if ((KQF_OPT_MASK_DBG_CALL & opt) != 0) {
		OutputDebugStringA(lpOutputString);
	}
}


////////////////////////////////////////////////////////////////////////////////
//
//                             Common window hooks
//

static BOOL WINAPI USER32_DestroyWindow(HWND hWnd)
{
	BOOL result;
	if (NULL == hWnd) {
		SetLastError(ERROR_INVALID_WINDOW_HANDLE);
		result = FALSE;
	} else {
		result = DestroyWindow(hWnd);
		if (hWnd == app_window) {
			app_window = NULL;
		} else if (hWnd == video_window) {
			video_window = NULL;
		}
	}
	KQF_TRACE("DestroyWindow<%#08lx>(%#08lx)[%i]\n", ReturnAddress, hWnd, result);
	return (result);
}


////////////////////////////////////////////////////////////////////////////////
//
//                  Write memory dump on unhandled exceptions
//

typedef BOOL (WINAPI *PFNMINIDUMPWRITEDUMP)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION, PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
static PFNMINIDUMPWRITEDUMP         crash_dump_write /* = NULL */;
static CHAR                         crash_dump_file[MAX_PATH] /* = {'\0'} */;
static LPTOP_LEVEL_EXCEPTION_FILTER crash_dump_next /* = NULL */;
static LONG /*volatile*/            crash_dump_done /* = 0 */;

static
DWORD WINAPI crash_dump_save(LPVOID lpParameter)
{
	BOOL Saved = FALSE;
	PMINIDUMP_EXCEPTION_INFORMATION const ExceptionParam = lpParameter;
	if (crash_dump_write && crash_dump_file[0]) {
		HANDLE File = CreateFileA(crash_dump_file, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (File != INVALID_HANDLE_VALUE) {
			MINIDUMP_TYPE const DumpType = kqf_get_opt(KQF_CFGO_CRASH_DUMP) - 1;
			Saved = crash_dump_write(GetCurrentProcess(), GetCurrentProcessId(), File, DumpType, ExceptionParam, NULL, NULL);
			CloseHandle(File);
			if (Saved) {
				InterlockedIncrement(&crash_dump_done);
			} else {
				DeleteFileA(crash_dump_file);
			}
		}
	}
	return (Saved);
}

static
LONG WINAPI crash_dump_proc(PEXCEPTION_POINTERS ExceptionInfo)
{
	if (ExceptionInfo) {
		PEXCEPTION_RECORD ExceptionRecord = ExceptionInfo->ExceptionRecord;
		if (ExceptionRecord) {
			kqf_log(KQF_LOGL_EMERGENCY, "Unhandled exception: code %#lx, addr %#08lx, info[%#08lx %#08lx %#08lx %#08lx %#08lx]\n",
				ExceptionRecord->ExceptionCode,
				ExceptionRecord->ExceptionAddress,
				ExceptionRecord->ExceptionInformation[0],
				ExceptionRecord->ExceptionInformation[1],
				ExceptionRecord->ExceptionInformation[2],
				ExceptionRecord->ExceptionInformation[3],
				ExceptionRecord->ExceptionInformation[4]);
			kqf_flush_log();
		}
		if (!crash_dump_done) {
			MINIDUMP_EXCEPTION_INFORMATION ExceptionParam;
			ExceptionParam.ThreadId = GetCurrentThreadId();
			ExceptionParam.ExceptionPointers = ExceptionInfo;
			ExceptionParam.ClientPointers = FALSE;
			if (ExceptionRecord && (EXCEPTION_STACK_OVERFLOW == ExceptionRecord->ExceptionCode)) {
				DWORD ThreadId;
				HANDLE Thread = CreateThread(NULL, 0, crash_dump_save, &ExceptionParam, 0, &ThreadId);
				if (Thread) {
					WaitForSingleObject(Thread, INFINITE);
					CloseHandle(Thread);
				}
			} else {
				crash_dump_save(&ExceptionParam);
			}
			if (crash_dump_next && !IsBadCodePtr((FARPROC)crash_dump_next)) {
				return (crash_dump_next(ExceptionInfo));
			}
		}
	}
	return (EXCEPTION_CONTINUE_SEARCH);
}

static
void crash_dump_init(void)
{
	if (NULL == crash_dump_write) {
		HMODULE Module = LoadLibraryA("dbghelp.dll");
		if (NULL == Module)
			Module = LoadLibraryA("dbgcore.dll");
		if (Module)
			crash_dump_write = (PFNMINIDUMPWRITEDUMP)GetProcAddress(Module, "MiniDumpWriteDump");
	}
	if ('\0' == crash_dump_file[0]) {
		CHAR Name[MAX_PATH];
		DWORD Size = GetModuleFileNameA(NULL, Name, ARRAYSIZE(Name));
		if ((0 < Size) && (Size < ARRAYSIZE(Name))) {
			LPSTR Ext = &Name[Size];
			while (--Ext > Name) {
				if ('.' == *Ext) {
					break;
				}
				if (('\\' == *Ext) || ('/' == *Ext) || (':' == *Ext)) {
					Ext = &Name[Size];
					break;
				}
			}
			if (Ext - Name + sizeof(".kq8fix.dmp") <= ARRAYSIZE(Name)) {
				if (lstrcpyA(Ext, ".kq8fix.dmp") != NULL) {
					lstrcpyA(&crash_dump_file[0], Name);
					if (GetFileAttributesA(crash_dump_file) != INVALID_FILE_ATTRIBUTES) {
						DeleteFileA(crash_dump_file);
					}
				}
			}
		}
	}
	if (kqf_get_opt(KQF_CFGO_CRASH_DUMP) != KQF_OPT_CRASH_DUMP_NONE) {
		LPTOP_LEVEL_EXCEPTION_FILTER next = SetUnhandledExceptionFilter(crash_dump_proc);
		if (next != crash_dump_proc)
			crash_dump_next = next;
		kqf_log(KQF_LOGL_INFO, "SetUnhandledExceptionFilter: %#08lx -> %#08lx\n", next, crash_dump_proc);
	}
}

extern
int (__cdecl *_imp___XcptFilter)(unsigned long xcptnum, PEXCEPTION_POINTERS pxcptinfoptrs);
int  __cdecl MSVCRT__XcptFilter (unsigned long xcptnum, PEXCEPTION_POINTERS pxcptinfoptrs)
{
	if (runtime_active) {
		if (kqf_get_opt(KQF_CFGO_CRASH_DUMP) != KQF_OPT_CRASH_DUMP_NONE) {
			kqf_log(KQF_LOGL_NOTICE, "ExceptionFilter: %#08lx(%lu)\n", xcptnum, xcptnum);
			crash_dump_proc(pxcptinfoptrs);
		}
	}
	return (_imp___XcptFilter(xcptnum, pxcptinfoptrs));
}


////////////////////////////////////////////////////////////////////////////////
//
//              Manually apply AppCompat shim "SingleProcAffinity"
//
//  The required APIs are present in Windows Vista and newer. The affinity of
//  the process is limited to the processor that is currently active. Nothing
//  is done if the option is disabled or if the process already runs on 1 CPU.
//

static
void apply_single_proc(void)
{
	HANDLE const Kernel32 = GetModuleHandleA("KERNEL32");
	BOOL (WINAPI *const GetProcessAffinityMask)(HANDLE, PDWORD_PTR, PDWORD_PTR) =
		(BOOL (WINAPI *)(HANDLE, PDWORD_PTR, PDWORD_PTR))GetProcAddress(
			Kernel32, "GetProcessAffinityMask");
	DWORD (WINAPI *const GetCurrentProcessorNumber)(VOID) =
		(DWORD (WINAPI *)(VOID))GetProcAddress(
			Kernel32, "GetCurrentProcessorNumber");
	BOOL (WINAPI *const SetProcessAffinityMask)(HANDLE, DWORD_PTR) =
		(BOOL (WINAPI *)(HANDLE, DWORD_PTR))GetProcAddress(
			Kernel32, "SetProcessAffinityMask");
	if ((NULL == GetProcessAffinityMask) ||
	    (NULL == GetCurrentProcessorNumber) ||
	    (NULL == SetProcessAffinityMask)) {
		kqf_log(KQF_LOGL_INFO, "SingleProcAffinity: required API functions are not present\n");
	} else {
		DWORD_PTR ProcessAffinityMask = 0UL;
		DWORD_PTR SystemAffinityMask = 0UL;
		HANDLE const Process = GetCurrentProcess();
		if (!GetProcessAffinityMask(Process, &ProcessAffinityMask, &SystemAffinityMask)) {
			kqf_log(KQF_LOGL_NOTICE, "SingleProcAffinity: failed to retrieve processor affinity {%#lx}\n", GetLastError());
		} else if (0UL == (ProcessAffinityMask & (ProcessAffinityMask - 1UL))) {
			//NOTE: the power-of-two check also returns true if ProcessAffinityMask is 0
			kqf_log(KQF_LOGL_INFO, "SingleProcAffinity: process already runs on a designated processor (%#lx)\n", ProcessAffinityMask);
		} else {
			DWORD const CurrentProcessorNumber = GetCurrentProcessorNumber();
			DWORD_PTR const CurrentProcessorMask = (DWORD_PTR)1UL << CurrentProcessorNumber;
			if ((CurrentProcessorNumber >= sizeof(DWORD_PTR) * CHAR_BIT) ||
			    (0UL == (CurrentProcessorMask & ProcessAffinityMask))) {
				kqf_log(KQF_LOGL_NOTICE, "SingleProcAffinity: current processor (%lu) is not in process affinity (%#lx)\n", CurrentProcessorNumber, ProcessAffinityMask);
			} else if (!SetProcessAffinityMask(Process, CurrentProcessorMask)) {
				kqf_log(KQF_LOGL_ERROR, "SingleProcAffinity: failed to set the process affinity (%#lx,%#lx){%#lx}\n", CurrentProcessorMask, ProcessAffinityMask, GetLastError());
			} else {
				kqf_log(KQF_LOGL_INFO, "SingleProcAffinity: pinned process to current processor (%lu)\n", CurrentProcessorNumber);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
//
//                    Application import pointer redirection
//
//  The function just looks for pointers, not names. Therefore the hooks are
//  enabled and restored with the pointers that are imported by this module.
//

static
DWORD patch_ptr(ULONG_PTR *old_ptr, ULONG_PTR new_ptr)
{
	DWORD status;
	MEMORY_BASIC_INFORMATION info;
	if (!kqf_query_mem(old_ptr, info)) {
		status = GetLastError();
	} else if (info.RegionSize < sizeof(ULONG_PTR)) {
		status = ERROR_INVALID_ADDRESS;
	} else {
		DWORD read_only = info.Protect & (PAGE_NOACCESS | PAGE_READONLY | PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_GUARD);
		if (read_only && !VirtualProtect(old_ptr, sizeof(*old_ptr), PAGE_EXECUTE_READWRITE, &info.Protect)) {
			status = GetLastError();
		} else {
			*old_ptr = new_ptr;
			FlushInstructionCache(GetCurrentProcess(), old_ptr, sizeof(*old_ptr));
			if (read_only) {
				VirtualProtect(old_ptr, sizeof(*old_ptr), info.Protect, &info.Protect);
			}
			status = ERROR_SUCCESS;
		}
	}
	return (status);
}

static
DWORD patch_import(ULONG_PTR old_ptr, ULONG_PTR new_ptr, const char *name)
{
	DWORD result = ERROR_PROC_NOT_FOUND;
	IMAGE_IMPORT_DESCRIPTOR const *i = kqf_app.info.imports;
	kqf_log(KQF_LOGL_INFO, "hook: '%s' %#08lx -> %#08lx\n", name, old_ptr, new_ptr);\
	for (; i->FirstThunk; ++i) {
		PIMAGE_THUNK_DATA f = (PIMAGE_THUNK_DATA)((ULONG_PTR)kqf_app.info.base + i->FirstThunk);
		for(; f->u1.Function; ++f) {
			if (new_ptr == f->u1.Function) {
				if (ERROR_PROC_NOT_FOUND == result) {
					result = ERROR_SUCCESS;
				}
			} else if (old_ptr == f->u1.Function) {
				DWORD status = patch_ptr(&f->u1.Function, new_ptr);
				if (status != ERROR_SUCCESS) {
					result = status;
				} else if (ERROR_PROC_NOT_FOUND == result) {
					result = ERROR_SUCCESS;
				}
			}
		}
	}
	if (result != ERROR_SUCCESS) {
		kqf_log(KQF_LOGL_WARNING, "hook: '%s' not patched {%#lx}\n", name, result);
	}
	return (result);
}

#define HOOK_IMPORT(m, p) patch_import((ULONG_PTR)p, (ULONG_PTR)(m##_##p), #p)
#define UNHOOK_IMPORT(m, p) patch_import((ULONG_PTR)(m##_##p), (ULONG_PTR)p, #p)

extern
void (__cdecl *_imp____set_app_type)(int at);
void  __cdecl MSVCRT___set_app_type (int at)
{
	if (!runtime_active && (/*_GUI_APP*/2 == at)) {
		InterlockedIncrement(&runtime_active);
		kqf_init();
		{
			SYSTEMTIME now;
			DWORD ver;
			BYTE major, minor;
			WORD build;
			GetSystemTime(&now);
			ver = GetVersion();
			major = LOBYTE(ver);
			minor = HIBYTE(ver);
			build = HIWORD(ver);
			if (0x8000 & build)
				build = (major >= 4) ? 0 : build & 0x7FFF;
			kqf_log(KQF_LOGL_NOTICE, "runtime: version %u.%u.%u.%u (%s)\n", KQF_VERF_MAJOR, KQF_VERF_MINOR, KQF_VERF_PATCH, KQF_VERF_FLAGS, KQF_VERS_UIVER);
			kqf_log(KQF_LOGL_NOTICE, "runtime: system time %.4hu-%.2hu-%.2huT%.2hu:%.2hu:%.2huZ, reported Windows version: %u.%u.%u\n", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, major, minor, build);
			kqf_log(KQF_LOGL_NOTICE, "runtime: loaded by '%s' in '%s' (game: %i, base: %#08lx)\n", kqf_app.name, kqf_app.path, kqf_app.info.version, kqf_app.inst);
		}
		crash_dump_init();
		/*HMODULE hMciavi = LoadLibraryA("mciavi32.dll");
		if (hMciavi) {
			kqf_log(KQF_LOGL_NOTICE, "Successfully loaded mciavi32.dll\n");
		}
		else {
			kqf_log(KQF_LOGL_NOTICE, "Failed to load mciavi32.dll: %lu\n", GetLastError());
		}

		HMODULE hMciavi_early = GetModuleHandleA("mciavi32.dll");
		kqf_log(KQF_LOGL_NOTICE, "mciavi32.dll loaded at startup: %s\n", hMciavi_early ? "Yes" : "No");
		*/
		if (kqf_app.info.imports) {
			int tracing = (kqf_get_log_level() >= KQF_LOGL_TRACE);
			if (tracing || (kqf_get_opt(KQF_CFGO_MASK_DBG) != KQF_OPT_MASK_DBG_CALL)) {
				HOOK_IMPORT(KERNEL32, OutputDebugStringA);
			}
			if (tracing || kqf_get_opt(KQF_CFGO_SHIM_CBT)) {
				HOOK_IMPORT(USER32, SetWindowsHookExA);
				if (tracing) {
					HOOK_IMPORT(USER32, UnhookWindowsHookEx);
				}
			}
			if (tracing || kqf_get_opt(KQF_CFGO_SHIM_UNMAP)) {
				if (tracing) {
					HOOK_IMPORT(KERNEL32, MapViewOfFile);
				}
				HOOK_IMPORT(KERNEL32, UnmapViewOfFile);
			}
			if (tracing ||
			    kqf_get_opt(KQF_CFGO_SHIM_GDFS) ||
			    kqf_get_opt(KQF_CFGO_CDROM_SIZE) ||
			    kqf_get_opt(KQF_CFGO_CDROM_FAKE)) {
				HOOK_IMPORT(KERNEL32, GetDiskFreeSpaceA);
			}
			if (tracing || kqf_get_opt(KQF_CFGO_SHIM_GMEM)) {
				HOOK_IMPORT(KERNEL32, GlobalMemoryStatus);
			}
			if (tracing || kqf_get_opt(KQF_CFGO_SHIM_RMDIR)) {
				if (tracing) {
					HOOK_IMPORT(KERNEL32, FindFirstFileA);
				}
				HOOK_IMPORT(KERNEL32, FindNextFileA);
				HOOK_IMPORT(KERNEL32, FindClose);
				HOOK_IMPORT(KERNEL32, RemoveDirectoryA);
			}
			if (kqf_get_opt(KQF_CFGO_SHIM_FIND)) {
				if (!init_find_shim()) {
					kqf_set_opt(KQF_CFGO_SHIM_FIND, KQF_OPT_BOOL_FALSE);
				}
			}
			if (tracing ||
			    kqf_get_opt(KQF_CFGO_VIDEO_AVI) ||
			    kqf_get_opt(KQF_CFGO_VIDEO_NOBORDER)) {
				HOOK_IMPORT(MSVFW32, MCIWndCreateA);
			}
			if (tracing ||
			    kqf_get_opt(KQF_CFGO_VIDEO_NOAPPMOVE) ||
			    kqf_get_opt(KQF_CFGO_VIDEO_NOVIDMOVE)) {
				HOOK_IMPORT(USER32, MoveWindow);
			}
			if (tracing || kqf_get_opt(KQF_CFGO_GLIDE_DISABLE)) {
				HOOK_IMPORT(KERNEL32, LoadLibraryA);
			}
			{
				HOOK_IMPORT(USER32, AdjustWindowRect);
				HOOK_IMPORT(USER32, CreateWindowExA);
				HOOK_IMPORT(KERNEL32, GetProcAddress);
			}
			if (tracing || kqf_get_opt(KQF_CFGO_WINDOW_NOBORDER)) {
				HOOK_IMPORT(USER32, ClipCursor);
			}
			if (tracing) {
				HOOK_IMPORT(USER32, GetCursorPos);
				HOOK_IMPORT(USER32, SetCursorPos);
			}
			if (tracing) {
				HOOK_IMPORT(USER32, ShowCursor);
				HOOK_IMPORT(USER32, SetWindowPos);
			}
			{
				HOOK_IMPORT(KERNEL32, GetPrivateProfileStringA);
			}
			if (tracing || kqf_get_opt(KQF_CFGO_CDROM_FAKE)) {
				{
					// Limit log level to errors because only KQMOE_VERSION_11FG and
					// KQMOE_VERSION_13FGIS are importing these three API functions.
					KQF_LOGL_ level = kqf_get_log_level();
					if (level > KQF_LOGL_ERROR)
						kqf_set_log_level(KQF_LOGL_ERROR);
					HOOK_IMPORT(KERNEL32, GetLogicalDriveStringsA);
					HOOK_IMPORT(KERNEL32, GetDriveTypeA);
					HOOK_IMPORT(KERNEL32, GetVolumeInformationA);
					kqf_set_log_level(level);
				}
				HOOK_IMPORT(KERNEL32, CreateFileA);
				HOOK_IMPORT(KERNEL32, GetFileSize);
				HOOK_IMPORT(KERNEL32, CloseHandle);
			}
			if (tracing) {
				HOOK_IMPORT(GDI32, CreatePalette);
				HOOK_IMPORT(GDI32, SelectPalette);
				HOOK_IMPORT(GDI32, RealizePalette);
				HOOK_IMPORT(GDI32, AnimatePalette);
				HOOK_IMPORT(GDI32, GetSystemPaletteEntries);
				HOOK_IMPORT(USER32, GetDC);
				HOOK_IMPORT(USER32, ReleaseDC);
			}
			hook_GFXClearScreen();
			patch_D3DTotalVideoMemory();
			patch_BrightnessSlider();
			if (kqf_get_opt(KQF_CFGO_TEXT_HEBREW_RTL)) {
				//init_rtl_text();
			}
		}
		apply_single_proc();
		kqf_log(KQF_LOGL_NOTICE, "runtime: init done\n");
	}
	_imp____set_app_type(at);
}

extern
void (__cdecl *_imp__exit)(int _Code);
void  __cdecl MSVCRT_exit (int _Code)
{
	if (runtime_active) {
		kqf_flush_log();
	}
	_imp__exit(_Code);
}

extern
void (__cdecl *_imp___exit)(int _Code);
void  __cdecl MSVCRT__exit (int _Code)
{
	if (runtime_active) {
		kqf_flush_log();
	}
	_imp___exit(_Code);
}


////////////////////////////////////////////////////////////////////////////////
//
//                                 Entry point
//

BOOL APIENTRY DllMain(HMODULE Module, DWORD Reason, LPVOID Reserved)
{
	UNREFERENCED_PARAMETER(Reserved);
	switch (Reason) {
	case DLL_PROCESS_ATTACH:
		if (Module != NULL) {
			DisableThreadLibraryCalls(Module);
		}
		break;
	case DLL_PROCESS_DETACH:
		if (runtime_active) {
			InterlockedDecrement(&runtime_active);
			kqf_log(KQF_LOGL_NOTICE, "runtime: unloading\n");
			if (kqf_app.info.imports) {
				unhook_GFXClearScreen();
				UNHOOK_IMPORT(USER32, ReleaseDC);
				UNHOOK_IMPORT(USER32, GetDC);
				UNHOOK_IMPORT(GDI32, GetSystemPaletteEntries);
				UNHOOK_IMPORT(GDI32, AnimatePalette);
				UNHOOK_IMPORT(GDI32, RealizePalette);
				UNHOOK_IMPORT(GDI32, SelectPalette);
				UNHOOK_IMPORT(GDI32, CreatePalette);
				UNHOOK_IMPORT(KERNEL32, CloseHandle);
				UNHOOK_IMPORT(KERNEL32, GetFileSize);
				UNHOOK_IMPORT(KERNEL32, CreateFileA);
				{
					// Limit log level to errors because only KQMOE_VERSION_11FG and
					// KQMOE_VERSION_13FGIS are importing these three API functions.
					KQF_LOGL_ level = kqf_get_log_level();
					if (level > KQF_LOGL_ERROR)
						kqf_set_log_level(KQF_LOGL_ERROR);
					UNHOOK_IMPORT(KERNEL32, GetVolumeInformationA);
					UNHOOK_IMPORT(KERNEL32, GetDriveTypeA);
					UNHOOK_IMPORT(KERNEL32, GetLogicalDriveStringsA);
					kqf_set_log_level(level);
				}
				UNHOOK_IMPORT(KERNEL32, GetPrivateProfileStringA);
				UNHOOK_IMPORT(USER32, SetWindowPos);
				UNHOOK_IMPORT(USER32, ShowCursor);
				UNHOOK_IMPORT(USER32, SetCursorPos);
				UNHOOK_IMPORT(USER32, GetCursorPos);
				UNHOOK_IMPORT(USER32, ClipCursor);
				UNHOOK_IMPORT(KERNEL32, GetProcAddress);
				UNHOOK_IMPORT(USER32, CreateWindowExA);
				UNHOOK_IMPORT(USER32, AdjustWindowRect);
				UNHOOK_IMPORT(KERNEL32, LoadLibraryA);
				UNHOOK_IMPORT(USER32, MoveWindow);
				UNHOOK_IMPORT(MSVFW32, MCIWndCreateA);
				//UNHOOK_IMPORT(USER32, SendMessageA);
				free_talk_complete();
				free_find_shim();
				UNHOOK_IMPORT(KERNEL32, RemoveDirectoryA);
				UNHOOK_IMPORT(KERNEL32, FindClose);
				UNHOOK_IMPORT(KERNEL32, FindNextFileA);
				UNHOOK_IMPORT(KERNEL32, FindFirstFileA);
				UNHOOK_IMPORT(KERNEL32, GlobalMemoryStatus);
				UNHOOK_IMPORT(KERNEL32, GetDiskFreeSpaceA);
				UNHOOK_IMPORT(KERNEL32, UnmapViewOfFile);
				UNHOOK_IMPORT(KERNEL32, MapViewOfFile);
				UNHOOK_IMPORT(USER32, UnhookWindowsHookEx);
				UNHOOK_IMPORT(USER32, SetWindowsHookExA);
				UNHOOK_IMPORT(KERNEL32, OutputDebugStringA);
				//cleanup_rtl_text();
			}
			kqf_log(KQF_LOGL_NOTICE, "runtime: unload done\n");
			kqf_close_log();
		}
		break;
	}
	return (TRUE);
}

