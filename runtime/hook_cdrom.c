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
#include "hook_cdrom.h"

#include "hook_video.h"

#include "../common/kqf_cfg.h"
#include "../common/kqf_log.h"


#define CDROM_DETECT_NAME "iceworld\\resource.vol"


static struct FAKE_CDROM_FILE {
	const char *  path;
	unsigned long size;
} const fake_cdrom_files[11] = {
	{FAKE_CDROM "barren\\sound\\c_wtrdie.aud",    7246734},
	{FAKE_CDROM "daventry\\sound\\c_drwndi.aud",  5787566},
	{FAKE_CDROM "deadcity\\sound\\c_brndie.aud",  5239874},
	{FAKE_CDROM "game\\sound\\c_blddie.aud",      4778978},
	{FAKE_CDROM "game\\sound\\cr_baway.aud",        37755},
	{FAKE_CDROM "gnome\\sound\\c_hngdie.aud",     3235452},
	{FAKE_CDROM "iceworld\\resource.vol",        15279993},
	{FAKE_CDROM "iceworld\\sound\\c_flydie.aud",  4573247},
	{FAKE_CDROM "iceworld\\sound\\oh_fdie.aud",     76298},
	{FAKE_CDROM "swamp\\sound\\c_spkdie.aud",     4289787},
	{FAKE_CDROM "temple3\\8gui.vol",               411384}
};
static int fake_cdrom_file = -1;


static
int file_exists(char const *name)
{
	WIN32_FIND_DATAA file;
	HANDLE find = FindFirstFileA(name, &file);
	if (find != INVALID_HANDLE_VALUE) {
		FindClose(find);
		return (0 == (FILE_ATTRIBUTE_DIRECTORY & file.dwFileAttributes));
	}
	return (0);
}


////////////////////////////////////////////////////////////////////////////////
//
//             Override "Drive" in "Install" section of ./mask.inf
//
// If KQGame::PlayFromCD is enabled (defaults to 0 for demos and 1 for retails)
// the CD root path is read from the mask.inf in the current working directory.
// "<Drive>mask.inf" has to exist and has to contain at least eight characters.
//

DWORD WINAPI KERNEL32_GetPrivateProfileStringA(LPCSTR lpAppName, LPCSTR lpKeyName, LPCSTR lpDefault, LPSTR lpReturnedString, DWORD nSize, LPCSTR lpFileName)
{
	DWORD result;
	KQF_TRACE("GetPrivateProfileStringA<%#08lx>('%s','%s','%s','%s')\n", ReturnAddress, lpFileName, lpAppName, lpKeyName, lpDefault ? lpDefault : "");
	result = GetPrivateProfileStringA(lpAppName, lpKeyName, lpDefault, lpReturnedString, nSize, lpFileName);
	if (lpAppName && (0 == lstrcmpiA(lpAppName, "Install")) && lpKeyName && (0 == lstrcmpiA(lpKeyName, "Drive")) && lpReturnedString && nSize && lpFileName) {
		DWORD error = GetLastError();
		if ((lpDefault && *lpDefault) ? (0 == lstrcmpA(lpReturnedString, lpDefault)) : (0 == result)) {
			// config file not found or value is missing
			if (kqf_get_opt(KQF_CFGO_CDROM_FAKE)) {
				lstrcpynA(lpReturnedString, FAKE_CDROM, nSize);
				result = lstrlenA(lpReturnedString);
				kqf_log(KQF_LOGL_INFO, "mask.inf: Install.Drive defaults to '%s'\n", lpReturnedString);
			}
		} else {
			// verify value and override with existing or fake CD-ROM drive
			CHAR name[MAX_PATH];
			DWORD mode =
				SetErrorMode(SEM_NOOPENFILEERRORBOX);
				SetErrorMode(SEM_NOOPENFILEERRORBOX | mode);
			lstrcpyA(name, lpReturnedString);
			lstrcpynA(&name[result], CDROM_DETECT_NAME, ARRAYSIZE(name) - result);
			if (!file_exists(name)) {
				DWORD bytes = GetLogicalDriveStringsA(0, NULL);
				LPSTR buffer = (LPSTR)LocalAlloc(LMEM_FIXED | LMEM_ZEROINIT, bytes + 1);
				if (buffer) {
					DWORD size = GetLogicalDriveStringsA(bytes, buffer);
					if ((0 < size) && (size <= bytes)) {
						LPTSTR drive = buffer;
						while (*drive) {
							int len = lstrlenA(drive);
							if ((drive != buffer) && (DRIVE_CDROM == GetDriveTypeA(drive))) {
								lstrcpyA(name, drive);
								lstrcpynA(&name[len], CDROM_DETECT_NAME, ARRAYSIZE(name) - len);
								if (file_exists(name))
									break;
							}
							drive += len + 1;
						}
						if (*drive) {
							kqf_log(KQF_LOGL_INFO, "mask.inf: Install.Drive '%s' overriden with '%s'\n", lpReturnedString, drive);
							lstrcpynA(lpReturnedString, drive, nSize);
							result = lstrlenA(lpReturnedString);
						}
					}
					LocalFree((HLOCAL)buffer);
				}
			}
			if (kqf_get_opt(KQF_CFGO_CDROM_FAKE)) {
				lstrcpyA(name, lpReturnedString);
				lstrcpynA(&name[result], CDROM_DETECT_NAME, ARRAYSIZE(name) - result);
				if (!file_exists(name)) {
					lstrcpynA(lpReturnedString, FAKE_CDROM, nSize);
					result = lstrlenA(lpReturnedString);
					kqf_log(KQF_LOGL_INFO, "mask.inf: Install.Drive fallback to '%s'\n", lpReturnedString);
				}
			}
			SetErrorMode(mode);
		}
		SetLastError(error);
	}
	KQF_TRACE("GetPrivateProfileStringA<%#08lx>('%s','%s','%s','%s')[%lu,'%s']{%#lx}\n", ReturnAddress, lpFileName, lpAppName, lpKeyName, lpDefault ? lpDefault : "", result, lpReturnedString, result ? ERROR_SUCCESS : GetLastError());
	return (result);
}

DWORD WINAPI KERNEL32_GetLogicalDriveStringsA(DWORD nBufferLength, LPSTR lpBuffer)
{
	DWORD result;
	KQF_TRACE("GetLogicalDriveStringsA<%#08lx>(%lu,%#08lx)\n", ReturnAddress, nBufferLength, lpBuffer);
	result = GetLogicalDriveStringsA(nBufferLength, lpBuffer);
	if (result > nBufferLength) {
		result += sizeof("X:\\") + sizeof(FAKE_CDROM);
	} else if (result && lpBuffer && *lpBuffer) {
		DWORD insert = lstrlenA(lpBuffer) + 1;
		if (insert <= nBufferLength - result) {
			LPSTR pos;
			for (pos = &lpBuffer[result]; pos >= lpBuffer; --pos)
				pos[insert] = *pos;
			result += insert;
			if (kqf_get_opt(KQF_CFGO_CDROM_FAKE) && (sizeof(FAKE_CDROM) <= nBufferLength - result)) {
				LPSTR end = lpBuffer;
				while (*end)
					end += lstrlenA(end) + 1;
				for (pos = &lpBuffer[result]; pos >= end; --pos)
					pos[sizeof(FAKE_CDROM)] = *pos;
				lstrcpyA(end, FAKE_CDROM);
				result += sizeof(FAKE_CDROM);
			}
		}
	}
	if ((result < nBufferLength) && lpBuffer) {
		LPCSTR drive;
		kqf_log(KQF_LOGL_DEBUG, "GetLogicalDriveStringsA:");
		for (drive = lpBuffer; *drive != '\0'; drive += lstrlenA(drive) + 1)
			kqf_log(KQF_LOGL_DEBUG, " '%s'", drive);
		kqf_log(KQF_LOGL_DEBUG, "\n");
	}
	KQF_TRACE("GetLogicalDriveStringsA<%#08lx>(%lu,%#08lx)[%ul]{%#lx}\n", ReturnAddress, nBufferLength, lpBuffer, result, GetLastError());
	return (result);
}

UINT WINAPI KERNEL32_GetDriveTypeA(LPCSTR lpRootPathName)
{
	UINT result;
	KQF_TRACE("GetDriveTypeA<%#08lx>('%s')\n", ReturnAddress, lpRootPathName);
	if (kqf_get_opt(KQF_CFGO_CDROM_FAKE) && lpRootPathName && (0 == lstrcmpiA(lpRootPathName, FAKE_CDROM))) {
		kqf_log(KQF_LOGL_INFO, "cdrom: fake drive type\n");
		result = DRIVE_CDROM;
	} else {
		result = GetDriveTypeA(lpRootPathName);
	}
	KQF_TRACE("GetDriveTypeA<%#08lx>('%s')[%u]{%#lx}\n", ReturnAddress, lpRootPathName, result, (result == DRIVE_UNKNOWN) ? ERROR_SUCCESS : GetLastError());
	return (result);
}

BOOL WINAPI KERNEL32_GetVolumeInformationA(LPCSTR lpRootPathName, LPSTR lpVolumeNameBuffer, DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength, LPDWORD lpFileSystemFlags, LPSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize)
{
	BOOL result;
	KQF_TRACE("GetVolumeInformationA<%#08lx>('%s')\n", ReturnAddress, lpRootPathName);
	if (kqf_get_opt(KQF_CFGO_CDROM_FAKE) && lpRootPathName && (0 == lstrcmpiA(lpRootPathName, FAKE_CDROM))) {
		if (lpVolumeNameBuffer)
			lstrcpynA(lpVolumeNameBuffer, "MASK", nVolumeNameSize);
		if (lpVolumeSerialNumber)
			*lpVolumeSerialNumber = 'X8QK';
		if (lpMaximumComponentLength)
			*lpMaximumComponentLength = 110;
		if (lpFileSystemFlags)
			*lpFileSystemFlags = FILE_READ_ONLY_VOLUME | FILE_UNICODE_ON_DISK | FILE_CASE_SENSITIVE_SEARCH;
		if (lpFileSystemNameBuffer)
			lstrcpynA(lpFileSystemNameBuffer, "CDFS", nFileSystemNameSize);
		kqf_log(KQF_LOGL_INFO, "cdrom: fake drive info\n");
		result = TRUE;
	} else {
		result = GetVolumeInformationA(lpRootPathName, lpVolumeNameBuffer, nVolumeNameSize, lpVolumeSerialNumber, lpMaximumComponentLength, lpFileSystemFlags, lpFileSystemNameBuffer, nFileSystemNameSize);
	}
	KQF_TRACE("GetVolumeInformationA<%#08lx>('%s')[%i,'%s',%#lx,%lu,%#lx,'%s']{%#lx}\n", ReturnAddress, lpRootPathName, result, lpVolumeNameBuffer ? lpVolumeNameBuffer : "", lpVolumeSerialNumber ? *lpVolumeSerialNumber : 0UL, lpMaximumComponentLength ? *lpMaximumComponentLength : 0UL, lpFileSystemFlags ? *lpFileSystemFlags : 0UL, lpFileSystemNameBuffer ? lpFileSystemNameBuffer : "", result ? ERROR_SUCCESS : GetLastError());
	return (result);
}

HANDLE WINAPI KERNEL32_CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
	HANDLE result;
	KQF_TRACE("CreateFileA<%#08lx>('%s',%#lx,%#lx,%#08lx,%lu,%#lx,%#08lx)\n", ReturnAddress, lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
	if (kqf_get_opt(KQF_CFGO_CDROM_FAKE) && lpFileName) {
		CHAR root[sizeof(FAKE_CDROM)];
		fake_cdrom_file = -1;
		lstrcpynA(root, lpFileName, sizeof(FAKE_CDROM));
		if (0 == lstrcmpiA(root, FAKE_CDROM)) {
			if (OPEN_EXISTING == dwCreationDisposition) {
				int i;
				for (i = 0; i < ARRAYSIZE(fake_cdrom_files); ++i) {
					if (0 == lstrcmpiA(lpFileName, fake_cdrom_files[i].path)) {
						fake_cdrom_file = i;
						if (!file_exists(lpFileName)) {
							kqf_log(KQF_LOGL_INFO, "cdrom: fake '%s' open\n", lpFileName);
							SetLastError(ERROR_SUCCESS);
							return (NULL);
						}
						break;
					}
				}
			} else {
				kqf_log(KQF_LOGL_INFO, "cdrom: '%s' access denied\n", lpFileName);
				SetLastError(ERROR_ACCESS_DENIED);
				return (INVALID_HANDLE_VALUE);
			}
		}
	}
	result = CreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
	KQF_TRACE("CreateFileA<%#08lx>('%s',%#lx,%#lx,%#08lx,%lu,%#lx,%#08lx)[%#08lx]{%#lx}\n", ReturnAddress, lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile, result, (result != INVALID_HANDLE_VALUE) ? ERROR_SUCCESS : GetLastError());
	return (result);
}

DWORD WINAPI KERNEL32_GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh)
{
	DWORD result = 0;
	KQF_TRACE("GetFileSize<%#08lx>(%#08lx)\n", ReturnAddress, hFile);
	if (fake_cdrom_file >= 0) {
		if (NULL == hFile) {
			kqf_log(KQF_LOGL_INFO, "cdrom: fake '%s' size [%lu]\n", fake_cdrom_files[fake_cdrom_file].path, fake_cdrom_files[fake_cdrom_file].size);
			result = fake_cdrom_files[fake_cdrom_file].size;
			if (lpFileSizeHigh)
				*lpFileSizeHigh = 0UL;
			SetLastError(NO_ERROR);
		}
		fake_cdrom_file = -1;
	}
	if (0 == result) {
		result = GetFileSize(hFile, lpFileSizeHigh);
	}
	KQF_TRACE("GetFileSize<%#08lx>(%#08lx)[%#lx,%#lx]{%#lx}\n", ReturnAddress, hFile, result, lpFileSizeHigh ? *lpFileSizeHigh : 0UL, (result != INVALID_FILE_SIZE) ? NO_ERROR : GetLastError());
	return (result);
}

BOOL WINAPI KERNEL32_CloseHandle(HANDLE hObject)
{
	BOOL result = FALSE;
	KQF_TRACE("CloseHandle<%#08lx>(%#08lx)\n", ReturnAddress, hObject);
	if (fake_cdrom_file >= 0) {
		if (NULL == hObject) {
			kqf_log(KQF_LOGL_INFO, "cdrom: fake '%s' close\n", fake_cdrom_files[fake_cdrom_file].path);
			SetLastError(ERROR_SUCCESS);
			result = TRUE;
		}
		fake_cdrom_file = -1;
	}
	if (!result) {
		result = CloseHandle(hObject);
	}
	KQF_TRACE("CloseHandle<%#08lx>(%#08lx)[%i]{%#lx}\n", ReturnAddress, hObject, result, result ? ERROR_SUCCESS : GetLastError());
	return (result);
}
