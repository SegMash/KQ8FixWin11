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
#ifndef HOOK_SHIM_H_
#define HOOK_SHIM_H_

#include "../common/kqf_win.h"

#ifdef __cplusplus
extern "C" {
#endif


// KQF_CFGO_SHIM_CBT

HHOOK WINAPI USER32_SetWindowsHookExA(int idHook, HOOKPROC lpfn, HINSTANCE hmod, DWORD dwThreadId);
BOOL WINAPI USER32_UnhookWindowsHookEx(HHOOK hhk);


// KQF_CFGO_SHIM_UNMAP

LPVOID WINAPI KERNEL32_MapViewOfFile(HANDLE hFileMappingObject, DWORD dwDesiredAccess, DWORD dwFileOffsetHigh, DWORD dwFileOffsetLow, SIZE_T dwNumberOfBytesToMap);
BOOL WINAPI KERNEL32_UnmapViewOfFile(LPCVOID lpBaseAddress);


// KQF_CFGO_SHIM_GDFS, KQF_CFGO_CDROM_SIZE, KQF_CFGO_CDROM_FAKE

BOOL WINAPI KERNEL32_GetDiskFreeSpaceA(LPCSTR lpRootPathName, LPDWORD lpSectorsPerCluster, LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters, LPDWORD lpTotalNumberOfClusters);


// KQF_CFGO_SHIM_GMEM

VOID WINAPI KERNEL32_GlobalMemoryStatus(LPMEMORYSTATUS lpBuffer);


// KQF_CFGO_SHIM_RMDIR

HANDLE WINAPI KERNEL32_FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData);
BOOL WINAPI KERNEL32_FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData);
BOOL WINAPI KERNEL32_FindClose(HANDLE hFindFile);
BOOL WINAPI KERNEL32_RemoveDirectoryA(LPCSTR lpPathName);


// KQF_CFGO_SHIM_FIND

int init_find_shim(void);
void free_find_shim(void);

typedef struct MSVCRT__finddata_t {
	unsigned int      attrib;
	long int          time_create;
	long int          time_access;
	long int          time_write;
	unsigned long int size;
	char              name[260];
} MSVCRT__finddata_t;

int __cdecl MSVCRT__findfirst(char const *filespec, MSVCRT__finddata_t *fileinfo);
int __cdecl MSVCRT__findnext(int handle, MSVCRT__finddata_t *fileinfo);
int __cdecl MSVCRT__findclose(int handle);
int __cdecl MSVCRT_remove(char const *path);


#ifdef __cplusplus
}
#endif
#endif
