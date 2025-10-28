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
#include "hook_shim.h"

#include "../common/kqf_app.h"
#include "../common/kqf_cfg.h"
#include "../common/kqf_log.h"

#include "hook_cdrom.h"


////////////////////////////////////////////////////////////////////////////////
//
//                      Make global CBT hook thread-local
//
// The game uses a computer-based training (CBT) hook to initialize the window
// object during creation. The CBT hook and the window are created in the same
// thread, therefore limiting the hook to the current thread doesn't break the
// application logic. Callback functions for global hooks have to be in a DLL.
// Without this workaround the application crashes on Windows Vista and newer.
//

HHOOK WINAPI USER32_SetWindowsHookExA(int idHook, HOOKPROC lpfn, HINSTANCE hmod, DWORD dwThreadId)
{
	HHOOK result;
	KQF_TRACE("SetWindowsHookExA<%#08lx>(%i,%#08lx,%#08lx,%lu)\n", ReturnAddress, idHook, lpfn, hmod, dwThreadId);
	if (kqf_get_opt(KQF_CFGO_SHIM_CBT)) {
		if ((WH_CBT == idHook) && ((NULL == hmod) || (0 == dwThreadId))) {
			hmod = NULL;
			dwThreadId = GetCurrentThreadId();
			kqf_log(KQF_LOGL_INFO, "SetWindowsHookExA: CBT hook limited to current thread (%lu)\n", dwThreadId);
		}
	}
	result = SetWindowsHookExA(idHook, lpfn, hmod, dwThreadId);
	KQF_TRACE("SetWindowsHookExA<%#08lx>(%i,%#08lx,%#08lx,%#lx)[%#08lx]{%#lx}\n", ReturnAddress, idHook, lpfn, hmod, dwThreadId, result, result ? ERROR_SUCCESS : GetLastError());
	return (result);
}

BOOL WINAPI USER32_UnhookWindowsHookEx(HHOOK hhk)
{
	BOOL result;
	KQF_TRACE("UnhookWindowsHookEx<%#08lx>(%#08lx)\n", ReturnAddress, hhk);
	result = UnhookWindowsHookEx(hhk);
	KQF_TRACE("UnhookWindowsHookEx<%#08lx>(%#08lx)[%i]{%#lx}\n", ReturnAddress, hhk, result, result ? ERROR_SUCCESS : GetLastError());
	return (result);
}


////////////////////////////////////////////////////////////////////////////////
//
//                      AppCompat shim "KingsQuestMask"
//
// The game includes a global cleanup function for 'StreamIO' objects that is
// using a dynamic_cast<MemRWStream *> to automatically unmap all file views.
// While reading/loading a save 'MemRWStream' objects are created that do not
// own the memory, but point into the memory of another 'MemRWStream' object.
// On Windows 9x the API failed if the address is not the view's base. But on
// Windows NT the view is unmapped if the address is anywhere inside the view
// (the game later raises an access violation while reading released memory).
//
// Windows XP and newer provide an AppCompat shim "KingsQuestMask" that hooks
// MapViewOfFile/UnmapViewOfFile and maintains an address list to ensure that
// only previously mapped views are unmapped. But the shim is only applied if
// the filename is "Mask.exe" and is neither present in WinNT/Win2K nor Wine.
// This implementation makes sure that UnmapViewOfFile is only called for the
// base address of a memory region (avoids bugs due to previous MapViewOfFile
// calls that might have been missed and saves the list management overhead).
//

LPVOID WINAPI KERNEL32_MapViewOfFile(HANDLE hFileMappingObject, DWORD dwDesiredAccess, DWORD dwFileOffsetHigh, DWORD dwFileOffsetLow, SIZE_T dwNumberOfBytesToMap)
{
	LPVOID result;
	KQF_TRACE("MapViewOfFile<%#08lx>(%#08lx,%#lx,%#lx,%#lx,%#lx)\n", ReturnAddress, hFileMappingObject, dwDesiredAccess, dwFileOffsetHigh, dwFileOffsetLow, dwNumberOfBytesToMap);
	result = MapViewOfFile(hFileMappingObject, dwDesiredAccess, dwFileOffsetHigh, dwFileOffsetLow, dwNumberOfBytesToMap);
	KQF_TRACE("MapViewOfFile<%#08lx>(%#08lx,%#lx,%#lx,%#lx,%#lx)[%#08lx]{%#lx}\n", ReturnAddress, hFileMappingObject, dwDesiredAccess, dwFileOffsetHigh, dwFileOffsetLow, dwNumberOfBytesToMap, result, result ? ERROR_SUCCESS : GetLastError());
	return (result);
}

BOOL WINAPI KERNEL32_UnmapViewOfFile(LPCVOID lpBaseAddress)
{
	BOOL result = TRUE;
	KQF_TRACE("UnmapViewOfFile<%#08lx>(%#08lx)\n", ReturnAddress, lpBaseAddress);
	if (kqf_get_opt(KQF_CFGO_SHIM_UNMAP)) {
		if (!lpBaseAddress) {
			kqf_log(KQF_LOGL_INFO, "UnmapViewOfFile: ignored NULL pointer\n");
			result = FALSE;
		} else {
			MEMORY_BASIC_INFORMATION mem;
			if (kqf_query_mem(lpBaseAddress, mem) && (MEM_MAPPED & mem.Type) && (lpBaseAddress != mem.AllocationBase)) {
				kqf_log(KQF_LOGL_INFO, "UnmapViewOfFile: ignored savegame subchunk (%#08lx,%#08lx)\n", mem.AllocationBase, lpBaseAddress);
				result = FALSE;
			}
		}
	}
	if (!result) {
		SetLastError(ERROR_INVALID_ADDRESS);
	} else {
		result = UnmapViewOfFile(lpBaseAddress);
	}
	KQF_TRACE("UnmapViewOfFile<%#08lx>(%#08lx)[%i]{%#lx}\n", ReturnAddress, lpBaseAddress, result, result ? ERROR_SUCCESS : GetLastError());
	return (result);
}


////////////////////////////////////////////////////////////////////////////////
//
//             Signed integer comparison in free disk space check
//
// The game checks the free disk space before starting a new game (4 MiB) and
// before unpacking a world/level (110 MiB - patch/resources/...). Due to the
// signed integer comparison the check fails if more than 2 GiB are available
// (NumberOfFreeClusters * SectorsPerCluster * BytesPerSector).
//
// The CD-ROM check of the European Release/Update verifies that the CD drive
// has no free clusters and that the total size is between 670 and 685 MiB...
// and last but not least, we have to support the fake CD-ROM directory here.
//

BOOL WINAPI KERNEL32_GetDiskFreeSpaceA(LPCSTR lpRootPathName, LPDWORD lpSectorsPerCluster, LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters)
{
	BOOL result;
	DWORD number[4];
	KQF_TRACE("GetDiskFreeSpaceA<%#08lx>('%s',%#08lx,%#08lx,%#08lx,%#08lx)\n", ReturnAddress, lpRootPathName, lpSectorsPerCluster, lpBytesPerSector, lpNumberOfFreeClusters, lpTotalNumberOfClusters);
	if (!lpSectorsPerCluster)     lpSectorsPerCluster     = &number[0]; *lpSectorsPerCluster     = 0;
	if (!lpBytesPerSector)        lpBytesPerSector        = &number[1]; *lpBytesPerSector        = 0;
	if (!lpNumberOfFreeClusters)  lpNumberOfFreeClusters  = &number[2]; *lpNumberOfFreeClusters  = 0;
	if (!lpTotalNumberOfClusters) lpTotalNumberOfClusters = &number[3]; *lpTotalNumberOfClusters = 0;
	if (kqf_get_opt(KQF_CFGO_CDROM_FAKE) && lpRootPathName && (0 == lstrcmpiA(lpRootPathName, FAKE_CDROM))) {
		*lpSectorsPerCluster     = 16;
		*lpBytesPerSector        = 2048;
		*lpNumberOfFreeClusters  = 0;
		*lpTotalNumberOfClusters = (670 * 1024*1024) / 2048 / 16;
		result = TRUE;
	} else {
		DWORD ErrorMode =
			SetErrorMode(SEM_NOOPENFILEERRORBOX);
			SetErrorMode(SEM_NOOPENFILEERRORBOX | ErrorMode);
		result = GetDiskFreeSpaceA(lpRootPathName, lpSectorsPerCluster, lpBytesPerSector, lpNumberOfFreeClusters, lpTotalNumberOfClusters);
		{
			DWORD ErrorCode = GetLastError();
			if (kqf_get_opt(KQF_CFGO_CDROM_SIZE) && result && lpRootPathName && (DRIVE_CDROM == GetDriveTypeA(lpRootPathName))) {
				DWORD TotalBytes = *lpTotalNumberOfClusters * *lpSectorsPerCluster * *lpBytesPerSector;
				if (*lpNumberOfFreeClusters) {
					kqf_log(KQF_LOGL_INFO, "GetDiskFreeSpaceA: number of free clusters overridden for CD-ROM ('%s')\n", lpRootPathName);
					*lpNumberOfFreeClusters = 0;
				}
				if ((TotalBytes < 670 * 1024*1024) || (685 * 1024*1024 < TotalBytes)) {
					kqf_log(KQF_LOGL_INFO, "GetDiskFreeSpaceA: total number of clusters overridden for CD-ROM ('%s')\n", lpRootPathName);
					*lpSectorsPerCluster     = 16;
					*lpBytesPerSector        = 2048;
					*lpTotalNumberOfClusters = (670 * 1024*1024) / 2048 / 16;
				}
			}
			SetErrorMode(ErrorMode);
			SetLastError(ErrorCode);
		}
		if (kqf_get_opt(KQF_CFGO_SHIM_GDFS) && result && *lpSectorsPerCluster && *lpBytesPerSector) {
			DWORD ClusterLimit = MAXLONG / *lpSectorsPerCluster / *lpBytesPerSector;
			if (*lpNumberOfFreeClusters > ClusterLimit) {
				kqf_log(KQF_LOGL_INFO, "GetDiskFreeSpaceA: number of free clusters limited to 2 GiB (%lu,%lu,'%s')\n", *lpNumberOfFreeClusters, ClusterLimit, lpRootPathName);
				*lpNumberOfFreeClusters = ClusterLimit;
			}
			if (*lpTotalNumberOfClusters > ClusterLimit) {
				kqf_log(KQF_LOGL_INFO, "GetDiskFreeSpaceA: total number of clusters limited to 2 GiB (%lu,%lu,'%s')\n", *lpTotalNumberOfClusters, ClusterLimit, lpRootPathName);
				*lpTotalNumberOfClusters = ClusterLimit;
			}
		}
	}
	KQF_TRACE("GetDiskFreeSpaceA<%#08lx>('%s',%lu,%lu,%lu,%lu)[%i]{%#lx}\n", ReturnAddress, lpRootPathName, *lpSectorsPerCluster, *lpBytesPerSector, *lpNumberOfFreeClusters, *lpTotalNumberOfClusters, result, result ? ERROR_SUCCESS : GetLastError());
	return (result);
}


////////////////////////////////////////////////////////////////////////////////
//
//       "KQGame::runOptimal" initialization (signed division/comparison)
//

VOID WINAPI KERNEL32_GlobalMemoryStatus(LPMEMORYSTATUS lpBuffer)
{
	KQF_TRACE("GlobalMemoryStatus<%#08lx>(%#08lx)\n", ReturnAddress, lpBuffer);
	if (!lpBuffer) {
		SetLastError(ERROR_INVALID_PARAMETER);
	} else {
		GlobalMemoryStatus(lpBuffer);
		KQF_TRACE("GlobalMemoryStatus<%#08lx>(%#08lx)[%lu,%lu,%#lx,%#lx,%#lx,%#lx,%#lx,%#lx]\n", ReturnAddress, lpBuffer, lpBuffer->dwLength, lpBuffer->dwMemoryLoad, lpBuffer->dwTotalPhys, lpBuffer->dwAvailPhys, lpBuffer->dwTotalPageFile, lpBuffer->dwAvailPageFile, lpBuffer->dwTotalVirtual, lpBuffer->dwAvailVirtual);
		if (kqf_get_opt(KQF_CFGO_SHIM_GMEM)) {
			if (lpBuffer->dwTotalPhys > MAXLONG)
				kqf_log(KQF_LOGL_INFO, "GlobalMemoryStatus: total physical memory limited to 2 GiB (%#lx)\n", lpBuffer->dwTotalPhys);
			if (lpBuffer->dwTotalPhys     > MAXLONG) lpBuffer->dwTotalPhys     = MAXLONG;
			if (lpBuffer->dwAvailPhys     > MAXLONG) lpBuffer->dwAvailPhys     = MAXLONG;
			if (lpBuffer->dwTotalPageFile > MAXLONG) lpBuffer->dwTotalPageFile = MAXLONG;
			if (lpBuffer->dwAvailPageFile > MAXLONG) lpBuffer->dwAvailPageFile = MAXLONG;
			if (lpBuffer->dwTotalVirtual  > MAXLONG) lpBuffer->dwTotalVirtual  = MAXLONG;
			if (lpBuffer->dwAvailVirtual  > MAXLONG) lpBuffer->dwAvailVirtual  = MAXLONG;
			KQF_TRACE("GlobalMemoryStatus<%#08lx>(%#08lx)[%lu,%lu,%#lx,%#lx,%#lx,%#lx,%#lx,%#lx]\n", ReturnAddress, lpBuffer, lpBuffer->dwLength, lpBuffer->dwMemoryLoad, lpBuffer->dwTotalPhys, lpBuffer->dwAvailPhys, lpBuffer->dwTotalPageFile, lpBuffer->dwAvailPageFile, lpBuffer->dwTotalVirtual, lpBuffer->dwAvailVirtual);
		}
	}
}


////////////////////////////////////////////////////////////////////////////////
//
//            Compatibility handling while removing directories
//
// FindNextFile, FindClose, and RemoveDirectory return 1 (TRUE) on success.
// FindNextFile and FindClose are checking the find handle. RemoveDirectory
// explicitly sets the error code on success (ERROR_SUCCESS) and on failure
// some error codes are replaced with ERROR_PATH_NOT_FOUND (because this is
// the only error code that the game checks to detect missing directories).
//

HANDLE WINAPI KERNEL32_FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData)
{
	HANDLE result;
	KQF_TRACE("FindFirstFileA<%#08lx>('%s')\n", ReturnAddress, lpFileName);
	result = FindFirstFileA(lpFileName, lpFindFileData);
	if ((INVALID_HANDLE_VALUE == result) && lpFindFileData) {
		lpFindFileData->dwFileAttributes = 0;
		lpFindFileData->cFileName[0] = '?';
		lpFindFileData->cFileName[1] = '\0';
	}
	KQF_TRACE("FindFirstFileA<%#08lx>('%s')[%#08lx,%#lx,'%s']{%#lx}\n", ReturnAddress, lpFileName, result, lpFindFileData ? lpFindFileData->dwFileAttributes : 0UL, lpFindFileData ? lpFindFileData->cFileName : "", result != INVALID_HANDLE_VALUE ? ERROR_SUCCESS : GetLastError());
	return (result);
}

BOOL WINAPI KERNEL32_FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData)
{
	BOOL result;
	KQF_TRACE_FIND_N("FindNextFileA<%#08lx>(%#08lx)\n", ReturnAddress, hFindFile);
	if (kqf_get_opt(KQF_CFGO_SHIM_RMDIR) && (INVALID_HANDLE_VALUE == hFindFile)) {
		kqf_log(KQF_LOGL_INFO, "FindNextFileA: ignored invalid handle\n");
		SetLastError(ERROR_INVALID_HANDLE);
		result = 0;
	} else {
		result = FindNextFileA(hFindFile, lpFindFileData);
	}
	if (!result && lpFindFileData) {
		lpFindFileData->dwFileAttributes = 0;
		lpFindFileData->cFileName[0] = '?';
		lpFindFileData->cFileName[1] = '\0';
	}
	KQF_TRACE_FIND_N("FindNextFileA<%#08lx>(%#08lx)[%i,%#lx,'%s']{%#lx}\n", ReturnAddress, hFindFile, result, lpFindFileData ? lpFindFileData->dwFileAttributes : 0UL, lpFindFileData ? lpFindFileData->cFileName : "", result ? ERROR_SUCCESS : GetLastError());
	if (kqf_get_opt(KQF_CFGO_SHIM_RMDIR) && result)
		result = TRUE;
	return (result);
}

BOOL WINAPI KERNEL32_FindClose(HANDLE hFindFile)
{
	BOOL result;
	KQF_TRACE("FindClose<%#08lx>(%#08lx)\n", ReturnAddress, hFindFile);
	if (kqf_get_opt(KQF_CFGO_SHIM_RMDIR) && (INVALID_HANDLE_VALUE == hFindFile)) {
		kqf_log(KQF_LOGL_INFO, "FindClose: ignored invalid handle\n");
		SetLastError(ERROR_INVALID_HANDLE);
		result = 0;
	} else {
		result = FindClose(hFindFile);
	}
	KQF_TRACE("FindClose<%#08lx>(%#08lx)[%i]{%#lx}\n", ReturnAddress, hFindFile, result, result ? ERROR_SUCCESS : GetLastError());
	if (kqf_get_opt(KQF_CFGO_SHIM_RMDIR) && result)
		result = TRUE;
	return (result);
}

BOOL WINAPI KERNEL32_RemoveDirectoryA(LPCSTR lpPathName)
{
	BOOL result;
	KQF_TRACE("RemoveDirectoryA<%#08lx>(%s)\n", ReturnAddress, lpPathName);
	result = RemoveDirectoryA(lpPathName);
	if (kqf_get_opt(KQF_CFGO_SHIM_RMDIR)) {
		if (result) {
			if (GetLastError() != ERROR_SUCCESS) {
				kqf_log(KQF_LOGL_INFO, "RemoveDirectoryA: error code reset on success\n");
				SetLastError(ERROR_SUCCESS);
			}
		} else {
			DWORD ErrorCode = GetLastError();
			switch (ErrorCode) {
			case ERROR_FILE_NOT_FOUND:
			case ERROR_NOT_READY:
			case ERROR_BAD_NETPATH:
			case ERROR_BAD_NET_NAME:
			case ERROR_DIRECTORY:
				kqf_log(KQF_LOGL_INFO, "RemoveDirectoryA: error code mapped to 'path not found (%#lx)'\n", ErrorCode);
				SetLastError(ERROR_PATH_NOT_FOUND);
				break;
			case ERROR_PATH_NOT_FOUND:
			case ERROR_ACCESS_DENIED:
			case ERROR_DIR_NOT_EMPTY:
			case ERROR_IS_SUBST_PATH:
				kqf_log(KQF_LOGL_DEBUG, "RemoveDirectoryA: failed to remove '%s' (%#lx)\n", lpPathName, ErrorCode);
				break;
			default:
				kqf_log(KQF_LOGL_NOTICE, "RemoveDirectoryA: failed to remove '%s' (%#lx)\n", lpPathName, ErrorCode);
				break;
			}
		}
	}
	KQF_TRACE("RemoveDirectoryA<%#08lx>(%s)[%i]{%#lx}\n", ReturnAddress, lpPathName, result, result ? ERROR_SUCCESS : GetLastError());
	if (kqf_get_opt(KQF_CFGO_SHIM_RMDIR) && result)
		result = TRUE;
	return (result);
}


////////////////////////////////////////////////////////////////////////////////
//
//                    Fix find handles that are closed twice
//

static
DWORD closed_find = TLS_OUT_OF_INDEXES;

static
int get_closed_find(void)
{
	return ((int)TlsGetValue(closed_find) - 1);
}

static
void set_closed_find(int find)
{
	TlsSetValue(closed_find, (LPVOID)(find + 1));
}

int init_find_shim(void)
{
	if (TLS_OUT_OF_INDEXES == closed_find) {
		closed_find = TlsAlloc();
		if (TLS_OUT_OF_INDEXES == closed_find) {
			kqf_log(KQF_LOGL_ERROR, "init_find_shim: failed to allocate TLS index (%#lx)\n", GetLastError());
		}
	}
	return (closed_find != TLS_OUT_OF_INDEXES);
}

void free_find_shim(void)
{
	if (closed_find != TLS_OUT_OF_INDEXES) {
		TlsFree(closed_find), closed_find = TLS_OUT_OF_INDEXES;
	}
}

extern
int (__cdecl *_imp___findfirst)(char const *filespec, MSVCRT__finddata_t *fileinfo);
int  __cdecl MSVCRT__findfirst (char const *filespec, MSVCRT__finddata_t *fileinfo)
{
	int result = _imp___findfirst(filespec, fileinfo);
	if (runtime_active) {
		if (-1 == result) {
			if (fileinfo) {
				fileinfo->attrib = 0;
				fileinfo->name[0] = '?';
				fileinfo->name[1] = '\0';
			}
		} else if (kqf_get_opt(KQF_CFGO_SHIM_FIND)) {
			set_closed_find(-1);
		}
		KQF_TRACE("_findfirst<%#08lx>('%s')[%i,%#x,'%s']{%i}\n", ReturnAddress, filespec, result, fileinfo ? fileinfo->attrib : 0U, fileinfo ? fileinfo->name : "", (result != -1) ? 0 : MSVCRT_errno);
	}
	return (result);
}

extern
int (__cdecl *_imp___findnext)(int handle, MSVCRT__finddata_t *fileinfo);
int  __cdecl MSVCRT__findnext (int handle, MSVCRT__finddata_t *fileinfo)
{
	int result;
	if (!runtime_active) {
		result = _imp___findnext(handle, fileinfo);
	} else {
		if (kqf_get_opt(KQF_CFGO_SHIM_FIND) && (get_closed_find() == handle)) {
			kqf_log(KQF_LOGL_INFO, "_findnext: ignored closed handle (%i)\n", handle);
			MSVCRT_errno = ENOENT;
			result = -1;
		} else {
			result = _imp___findnext(handle, fileinfo);
		}
		if ((-1 == result) && fileinfo) {
			fileinfo->attrib = 0;
			fileinfo->name[0] = '?';
			fileinfo->name[1] = '\0';
		}
		KQF_TRACE_FIND_N("_findnext<%#08lx>(%i)[%i,%#x,'%s']{%i}\n", ReturnAddress, handle, result, fileinfo ? fileinfo->attrib : 0U, fileinfo ? fileinfo->name : "", (result != -1) ? 0 : MSVCRT_errno);
	}
	return (result);
}

extern
int (__cdecl *_imp___findclose)(int handle);
int  __cdecl MSVCRT__findclose (int handle)
{
	int result;
	if (!runtime_active) {
		result = _imp___findclose(handle);
	} else {
		if (-1 == handle) {
			kqf_log(KQF_LOGL_INFO, "_findclose: ignored invalid handle\n");
			MSVCRT_errno = EINVAL;
			result = -1;
		} else if (kqf_get_opt(KQF_CFGO_SHIM_FIND) && (get_closed_find() == handle)) {
			kqf_log(KQF_LOGL_INFO, "_findclose: ignored closed handle (%i)\n", handle);
			result = 0;
		} else {
			result = _imp___findclose(handle);
			if (kqf_get_opt(KQF_CFGO_SHIM_FIND) && (result != -1)) {
				set_closed_find(handle);
			}
		}
		KQF_TRACE("_findclose<%#08lx>(%i)[%i]{%i}\n", ReturnAddress, handle, result, (result != -1) ? 0 : MSVCRT_errno);
	}
	return (result);
}

extern
int (__cdecl *_imp__remove)(char const *path);
int  __cdecl MSVCRT_remove (char const *path)
{
	int result = _imp__remove(path);
	if (runtime_active) {
		KQF_TRACE("remove<%#08lx>('%s')[%i]{%i}\n", ReturnAddress, path, result, (result != -1) ? 0 : MSVCRT_errno);
	}
	return (result);
}
