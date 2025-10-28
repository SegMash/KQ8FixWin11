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
#include "setup.rh"

#include "../common/kqf_app.h"
#include "../common/kqf_cfg.h"
#include "../common/kqf_log.h"
#include "../common/kqf_init.h"
#include "../common/kqf_win.h"


typedef struct EDITCOOKIE {
	BYTE const *data;
	LONG        size;
	LONG        pos;
} EDITCOOKIE;

static
DWORD CALLBACK edit_stream_callback(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb)
{
	LONG size;
	EDITCOOKIE *const cookie = (EDITCOOKIE *)dwCookie;

	if (!cookie)
		return (ERROR_INVALID_PARAMETER);
	if (!cookie->data)
		return (ERROR_NO_DATA);

	size = cookie->size - cookie->pos;
	if (size > cb)
		size = cb;
	*pcb = size;
	if (size > 0) {
		BYTE const *data = cookie->data + cookie->pos;
		while (size--)
			*pbBuff++ = *data++;
		cookie->pos += *pcb;
	}
	return (ERROR_SUCCESS);
}

static
DWORD edit_stream_in(HWND edit, LPCSTR data)
{
	EDITCOOKIE cookie;
	EDITSTREAM stream;

	if (!edit || !data || !IsWindow(edit))
		return (ERROR_INVALID_PARAMETER);

	cookie.data = (BYTE const *)data;
	cookie.size = lstrlen(data);
	cookie.pos = 0;
	stream.dwCookie = (DWORD_PTR)&cookie;
	stream.dwError = ERROR_NO_DATA;
	stream.pfnCallback = edit_stream_callback;
	SendMessageA(edit, EM_STREAMIN, SF_RTF, (LPARAM)&stream);
	return (stream.dwError);
}


typedef enum KQMOE_STATE_ {
	KQMOE_STATE_NONE      = INDEXTOSTATEIMAGEMASK(0),
	KQMOE_STATE_UNCHECKED = INDEXTOSTATEIMAGEMASK(1),
	KQMOE_STATE_CHECKED   = INDEXTOSTATEIMAGEMASK(2),
	KQMOE_STATE_MASK      = LVIS_STATEIMAGEMASK
} KQMOE_STATE_;

typedef struct KQMOE_BIN {
	struct KQMOE_BIN *next;
	struct KQMOE_BIN *prev;
	KQMOE_VERSION_    version;
	KQMOE_STATE_      state;
	char             *name;
	char              path[MAX_PATH];
} KQMOE_BIN;

static KQMOE_BIN *s_list /* = NULL */;
static LONG /*volatile*/ s_list_init /* = 0 */;


static
int error_msg(HWND dlg, UINT id, UINT type, DWORD error)
{
	CHAR text[1024];
	KQF_TRACE("error_msg: %u %#lx (%li)\n", id, error, error);
	if (0 >= LoadStringA(kqf_mod, id, text, ARRAYSIZE(text))) {
		kqf_log(KQF_LOGL_NOTICE, "error_msg: resource string not found (%u)\n", id);
		wsprintfA(text, "ERROR: %u", id);
	}
	if (error != 0 ) {
		CHAR desc[1024];
		LPSTR msg = NULL;
		if (0 == FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, error, 0, (LPSTR)&msg, 0, NULL)) {
			kqf_log(KQF_LOGL_NOTICE, "error_msg: error string for %#lx (%li) not found (%#lx)\n", error, error, GetLastError());
			msg = NULL;
		}
		wsprintfA(desc, "%s\n\n%#lx (%li): %s", text, error, error, msg);
		if (msg) {
			LocalFree((HLOCAL)msg);
		}
		lstrcpynA(text, desc, 1024);
	}
	return (MessageBoxA(dlg, text, NULL, type));
}


static
DWORD extract_hook(HWND dlg)
{
	char path[MAX_PATH];
	kqf_app_filepath(KQMOE_RT_FIXNEW, path);
	{
		HMODULE hook = LoadLibraryA(path);
		if (hook != NULL) {
			DLLGETVERSIONPROC DllGetVersion = (DLLGETVERSIONPROC)GetProcAddress(hook, "DllGetVersion");
			if (NULL == DllGetVersion) {
				kqf_log(KQF_LOGL_NOTICE, "extract_hook: shim runtime version callback missing\n");
			} else {
				DLLVERSIONINFO2 ver = {{sizeof(DLLVERSIONINFO2), 0, 0, 0, 0}, 0, 0};
				if (S_OK == DllGetVersion(&ver.info1)) {
					kqf_log(KQF_LOGL_INFO, "extract_hook: shim runtime already exists (%lu.%lu.%hu.%lu)\n",
						ver.info1.dwMajorVersion, ver.info1.dwMinorVersion, HIWORD(ver.ullVersion), ver.info1.dwBuildNumber);
					if ((ver.info1.dwBuildNumber == KQF_VERF_FLAGS) && (ver.ullVersion >= MAKEDLLVERULL(KQF_VERF_MAJOR, KQF_VERF_MINOR, KQF_VERF_PATCH, 0))) {
						FreeLibrary(hook);
						return (ERROR_SUCCESS);
					}
				}
			}
			FreeLibrary(hook);
		}
	}
	{
		DWORD status;
		HRSRC res_info = FindResourceA(NULL, MAKEINTRESOURCEA(IDX_RUNTIME), RT_RCDATA);
		if (NULL == res_info) {
			status = GetLastError();
			kqf_log(KQF_LOGL_ERROR, "extract_hook: shim runtime resource not found (%#lx)\n", status);
		} else {
			HGLOBAL res_data = LoadResource(NULL, res_info);
			if (NULL == res_data) {
				status = GetLastError();
				kqf_log(KQF_LOGL_ERROR, "extract_hook: failed to load shim runtime resource (%#lx)\n", status);
			} else {
				LPVOID data = LockResource(res_data);
				DWORD size = SizeofResource(NULL, res_info);
				HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
				if (INVALID_HANDLE_VALUE == file) {
					status = GetLastError();
					kqf_log(KQF_LOGL_ERROR, "extract_hook: failed to create shim runtime file (%#lx)\n", status);
				} else {
					DWORD written;
					if (!WriteFile(file, data, size, &written, NULL)) {
						status = GetLastError();
						kqf_log(KQF_LOGL_ERROR, "extract_hook: failed to write shim runtime file (%#lx)\n", status);
					} else if (written != size) {
						status = ERROR_WRITE_FAULT;
						kqf_log(KQF_LOGL_ERROR, "extract_hook: shim runtime file write fault\n");
					} else {
						status = ERROR_SUCCESS;
					}
					CloseHandle(file);
				}
			}
		}
		if ((dlg != NULL) && (status != ERROR_SUCCESS)) {
			error_msg(dlg, IDS_ERR_NO_EXTRACT, MB_ICONEXCLAMATION, status);
		}
		return (status);
	}
}


static
KQMOE_BIN *new_bin(char const path[MAX_PATH])
{
	KQMOE_BIN *bin;
	KQF_TRACE("new_bin('%s')\n", path);
	bin = (KQMOE_BIN *)LocalAlloc(LMEM_FIXED | LMEM_ZEROINIT, sizeof(*bin));
	if (NULL == bin) {
		kqf_log(KQF_LOGL_ERROR, "new_bin: failed to allocate entry for '%s' (%#lx)\n", path, GetLastError());
	} else {
		DWORD len = SearchPathA(NULL, path, NULL, MAX_PATH, bin->path, &bin->name);
		if ((len <= 0) && (MAX_PATH <= len)) {
			kqf_log(KQF_LOGL_ERROR, "new_bin: invalid filename '%s' (%#lx)\n", path, GetLastError());
			lstrcpyA(bin->path, path);
			bin->name = bin->path;
		}
		bin->version = KQMOE_VERSION_UNKNOWN;
		bin->state = KQMOE_STATE_NONE;
	}
	KQF_TRACE("new_bin('%s')[%#08lx,'%s']\n", path, bin, bin ? bin->name : "");
	return (bin);
}

static
KQMOE_STATE_ read_bin(KQMOE_BIN *bin)
{
	HANDLE file;
	KQF_TRACE("read_bin(%#08lx,'%s')\n", bin, bin->name);
	bin->version = KQMOE_VERSION_UNKNOWN;
	bin->state = KQMOE_STATE_NONE;
	file = CreateFileA(bin->path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (INVALID_HANDLE_VALUE == file) {
		kqf_log(KQF_LOGL_ERROR, "read_bin: failed to open '%s' (%#lx)\n", bin->name, GetLastError());
	} else {
		DWORD size = GetFileSize(file, NULL);
		LPVOID base = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		if (NULL == base) {
			kqf_log(KQF_LOGL_ERROR, "read_bin: failed to allocate memory for '%s' (%#lx)\n", bin->name, GetLastError());
		} else {
			DWORD read = 0;
			if (!ReadFile(file, base, size, &read, NULL) || (read != size)) {
				kqf_log(KQF_LOGL_ERROR, "read_bin: failed to read '%s' (%#lx)\n", bin->name, GetLastError());
			} else {
				KQMOE_INFO info;
				char const *name;
				if (!kqmoe_info(&info, base)) {
					kqf_log(KQF_LOGL_NOTICE, "read_bin: version detection failed for '%s'\n", bin->name);
				} else {
					bin->version = info.version;
				}
				name = kqmoe_rt(&info);
				if (NULL == name) {
					kqf_log(KQF_LOGL_NOTICE, "read_bin: run-time library import not found in '%s'\n", bin->name);
				} else if ((0 == lstrcmpiA(name, KQMOE_RT_MSVCRT)) || (0 == lstrcmpiA(name, KQMOE_RT_FIXOLD))) {
					bin->state = KQMOE_STATE_UNCHECKED;
				} else if (0 == lstrcmpiA(name, KQMOE_RT_FIXNEW)) {
					bin->state = KQMOE_STATE_CHECKED;
				} else {
					kqf_log(KQF_LOGL_ERROR, "read_bin: unexpected run-time library name in '%s'\n", bin->name);
				}
			}
			VirtualFree(base, 0, MEM_RELEASE);
		}
		CloseHandle(file);
	}
	KQF_TRACE("read_bin(%#08lx,'%s')[%i,%i]\n", bin, bin->name, bin->state, bin->version);
	return (bin->state);
}

static
DWORD write_bin(KQMOE_BIN *bin, KQMOE_STATE_ state)
{
	HANDLE file;
	DWORD status;
	KQF_TRACE("write_bin(%#08lx,'%s',%i,%i)\n", bin, bin->name, bin->state, state);
	file = CreateFileA(bin->path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (INVALID_HANDLE_VALUE == file) {
		status = GetLastError();
		kqf_log(KQF_LOGL_ERROR, "write_bin: failed to open '%s' (%#lx)\n", bin->name, status);
	} else {
		DWORD size = GetFileSize(file, NULL);
		LPVOID base = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		if (NULL == base) {
			status = GetLastError();
			kqf_log(KQF_LOGL_ERROR, "write_bin: failed to allocate memory for '%s' (%#lx)\n", bin->name, status);
		} else {
			DWORD read = 0;
			if (!ReadFile(file, base, size, &read, NULL) || (read != size)) {
				status = GetLastError();
				kqf_log(KQF_LOGL_ERROR, "write_bin: failed to read '%s' (%#lx)\n", bin->name, status);
			} else {
				KQMOE_INFO info;
				char const *name;
				if (!kqmoe_info(&info, base)) {
					kqf_log(KQF_LOGL_NOTICE, "write_bin: version detection failed for '%s'\n", bin->name);
				}
				name = kqmoe_rt(&info);
				if (NULL == name) {
					status = ERROR_NOT_FOUND;
					kqf_log(KQF_LOGL_NOTICE, "write_bin: run-time library import not found in '%s'\n", bin->name);
				} else {
					char const *new_name = name;
					switch (state) {
					case KQMOE_STATE_UNCHECKED:
						if (lstrcmpiA(name, KQMOE_RT_MSVCRT)) {
							new_name = KQMOE_RT_MSVCRT;
						}
						break;
					case KQMOE_STATE_CHECKED:
						if (lstrcmpiA(name, KQMOE_RT_FIXNEW)) {
							new_name = KQMOE_RT_FIXNEW;
						}
						break;
					}
					if (name == new_name) {
						status = RPC_S_ENTRY_ALREADY_EXISTS;
						kqf_log(KQF_LOGL_ERROR, "write_bin: unexpected state for '%s' (%#lx)\n", bin->name, state);
					} else {
						DWORD written = 0;
						lstrcpyA((char *)name, new_name);
						*(DWORD *)&info.header->OptionalHeader.CheckSum = 0;
						if (SetFilePointer(file, 0, NULL, FILE_BEGIN) || !WriteFile(file, base, size, &written, NULL) || (written != size)) {
							status = GetLastError();
							kqf_log(KQF_LOGL_ERROR, "write_bin: failed to write '%s' (%#lx)\n", bin->name, status);
						} else {
							bin->state = state;
							status = ERROR_SUCCESS;
						}
					}
				}
			}
			VirtualFree(base, 0, MEM_RELEASE);
		}
		CloseHandle(file);
	}
	KQF_TRACE("write_bin(%#08lx,'%s',%i,%i)[%#lx]\n", bin, bin->name, bin->state, state, status);
	return (status);
}


static
int is_kqmoe(char const path[MAX_PATH])
{
	int result;
	DWORD handle;
	DWORD size;
	KQF_TRACE("is_kqmoe('%s')\n", path);
	result = 0;
	handle = 0;
	size = GetFileVersionInfoSizeA(path, &handle);
	if (size > 0) {
		LPVOID data = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
		if (NULL == data) {
			kqf_log(KQF_LOGL_ERROR, "is_kqmoe: failed to allocate memory for '%s' (%#lx)\n", path, GetLastError());
		} else {
			if (!GetFileVersionInfoA(path, handle, size, data)) {
				kqf_log(KQF_LOGL_ERROR, "is_kqmoe: failed to query version info of '%s' (%#lx)\n", path, GetLastError());
			} else {
				LPCSTR const format = "\\StringFileInfo\\%04x%04x\\OriginalFilename";
				CHAR block[sizeof(    "\\StringFileInfo\\00000000\\OriginalFilename")];
				struct LANGANDCODEPAGE {
					WORD wLanguage;
					WORD wCodePage;
				} *lc = NULL;
				UINT length = 0;
				lstrcpynA(block, "\\VarFileInfo\\Translation", sizeof(block));
				if (VerQueryValueA(data, block, &lc, &length) && (lc != NULL) && (length >= sizeof(*lc))) {
					LPSTR value = NULL;
					length = 0;
					wsprintfA(block, format, lc->wLanguage, lc->wCodePage);
					result = VerQueryValueA(data, block, &value, &length) &&
						(value != NULL) && (length >= sizeof("Mask.exe")) &&
						(0 == lstrcmpiA(value, "Mask.exe"));
				}
			}
			VirtualFree(data, 0, MEM_RELEASE);
		}
	}
	KQF_TRACE("is_kqmoe('%s')[%i]\n", path, result);
	return (result);
}

static
KQMOE_BIN *init_list(void) {
	char path[MAX_PATH];
	HANDLE find;
	WIN32_FIND_DATAA file;
	KQF_TRACE("init_list()\n");
	kqf_app_filepath("*.exe", path);
	find = FindFirstFileA(path, &file);
	if (INVALID_HANDLE_VALUE == find) {
		kqf_log(KQF_LOGL_ERROR, "init_list: no '%s' found (%#lx)\n", path, GetLastError());
	} else {
		KQMOE_BIN *prev = NULL;
		KQMOE_BIN **next = &s_list;
		do {
			if ((0 == (FILE_ATTRIBUTE_DIRECTORY & file.dwFileAttributes)) &&
				(0 == file.nFileSizeHigh) && (file.nFileSizeLow <= 64 * 1024 * 1024)) {
				kqf_app_filepath(file.cFileName, path);
				if (is_kqmoe(path)) {
					*next = new_bin(path);
					if (*next != NULL) {
						read_bin(*next);
						(*next)->prev = prev;
						prev = *next;
						next = &prev->next;
					}
				}
			}
		} while (FindNextFileA(find, &file));
		FindClose(find);
	}
	KQF_TRACE("init_list()[%#08lx]\n", s_list);
	return (s_list);
}


static
void dlg_init_title(HWND Dlg)
{
	CHAR Caption[128];
	GetWindowTextA(Dlg, Caption, MAX_PATH - sizeof("( " KQF_VERS_UIVER ")"));
	lstrcatA(Caption, " (" KQF_VERS_UIVER ")");
	SetWindowTextA(Dlg, Caption);
	SendMessageA(Dlg, WM_SETICON, ICON_BIG, (LPARAM)LoadIconA(kqf_mod, MAKEINTRESOURCEA(IDI_MAIN)));
}

static
void dlg_init_combo(HWND ComboBox, UINT StrBase, UINT StrCount, int Index)
{
	UINT StrID;
	for (StrID = StrBase; StrID < StrBase + StrCount; ++StrID) {
		CHAR Str[64];
		if (LoadStringA(kqf_mod, StrID, Str, ARRAYSIZE(Str)) <= 0) {
			wsprintfA(Str, "%u", StrID - StrBase);
		}
		SendMessageA(ComboBox, CB_ADDSTRING, 0, (LPARAM)Str);
	}
	SendMessageA(ComboBox, CB_SETCURSEL, Index, 0);
	if (!SendMessageA(ComboBox, CB_SETMINVISIBLE, StrCount, 0)) {
		RECT Rect;
		LONG_PTR Height = SendMessageA(ComboBox, CB_GETITEMHEIGHT, 0, 0);
		if ((Height > 0) && GetWindowRect(ComboBox, &Rect)) {
			SetWindowPos(ComboBox, HWND_TOP, Rect.left, Rect.top,
				(Rect.right - Rect.left),
				(Rect.bottom - Rect.top) + (Height * (StrCount + 1)),
				SWP_NOZORDER | SWP_NOMOVE);
		}
	}
}

static
void dlg_init_check(HWND Dlg)
{
	int opt;
	for (opt = KQF_CFGO_SHIM_CBT; opt < KQF_CFGO_COUNT; ++opt) {
		SendMessageA(GetDlgItem(Dlg, IDC_OPT_BASE + opt), BM_SETCHECK, kqf_get_opt(opt) ? BST_CHECKED : BST_UNCHECKED, 0);
	}
}

static
void dlg_init_list_font(HWND Dlg, HWND List)
{
	HFONT Font = (HFONT)SendMessageA(Dlg, WM_GETFONT, 0, 0);
	if (Font) {
		LOGFONTA Desc;
		if (sizeof(LOGFONTA) == GetObjectA(Font, sizeof(LOGFONTA), &Desc)) {
			Desc.lfHeight += (0 == Desc.lfHeight) ? 12 : ((Desc.lfHeight > 0) ? +3 : -3);
			Desc.lfWidth = 0;
			Desc.lfWeight = FW_BOLD;
			Font = CreateFontIndirectA(&Desc);
			if (Font) {
				SendMessageA(List, WM_SETFONT, (WPARAM)Font, FALSE);
			}
		}
	}
}

static DWORD dlg_list_exstyle =
	LVS_EX_CHECKBOXES |
	LVS_EX_FULLROWSELECT |
	LVS_EX_ONECLICKACTIVATE;

static
void dlg_init_list_style(HWND List)
{
	SendMessageA(List, LVM_SETEXTENDEDLISTVIEWSTYLE, dlg_list_exstyle, dlg_list_exstyle);
	dlg_list_exstyle = (DWORD)SendMessageA(List, LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0);
	if (!(LVS_EX_CHECKBOXES & dlg_list_exstyle)) {
		HIMAGELIST ImageList = ImageList_LoadImageA(kqf_mod, MAKEINTRESOURCEA(IDB_CHECK), 0, 2, RGB(255, 0, 255), IMAGE_BITMAP, LR_DEFAULTCOLOR);
		if (ImageList) {
			SendMessageA(List, LVM_SETIMAGELIST, LVSIL_STATE, (LPARAM)ImageList);
		}
	}
}

static
void dlg_init_list_columns(HWND List)
{
	LVCOLUMNA Column;
	Column.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	Column.cx = 255;
	Column.pszText = "filename";
	Column.iSubItem = 0;
	SendMessageA(List, LVM_INSERTCOLUMNA, Column.iSubItem, (LPARAM)&Column);
	Column.cx = 178;
	Column.pszText = "version";
	Column.iSubItem = 1;
	SendMessageA(List, LVM_INSERTCOLUMNA, Column.iSubItem, (LPARAM)&Column);
}

static
void dlg_init_list(HWND Dlg)
{
	HWND List = GetDlgItem(Dlg, IDC_BIN_LIST);
	dlg_init_list_font(Dlg, List);
	dlg_init_list_style(List);
	dlg_init_list_columns(List);
	if (!s_list) {
		error_msg(Dlg, IDS_ERR_LIST_EMPTY, MB_ICONEXCLAMATION, 0);
	} else if (0 == InterlockedCompareExchange(&s_list_init, 1, 0)) {
		KQMOE_BIN *Bin;
		int Extract = 0;
		for (Bin = s_list; Bin; Bin = Bin->next) {
			int Index;
			LVITEMA Item = {LVIF_TEXT | LVIF_PARAM, 0, 0, 0, 0, NULL, 0, 0, (LPARAM)NULL};
			Item.pszText = Bin->name;
			Item.lParam = (LPARAM)Bin;
			Index = SendMessageA(List, LVM_INSERTITEMA, 0, (LPARAM)&Item);
			if (Index >= 0) {
				char Text[64];
				if (LoadStringA(kqf_mod, IDS_KQMOE_VERSION_BASE + Bin->version, Text, ARRAYSIZE(Text)) > 0) {
					Item.mask = LVIF_TEXT;
					Item.iItem = Index;
					Item.iSubItem = 1;
					Item.pszText = Text;
					SendMessageA(List, LVM_SETITEMTEXTA, Item.iItem, (LPARAM)&Item);
				}
				Item.mask = LVIF_STATE;
				Item.iItem = Index;
				Item.iSubItem = 0;
				Item.state = Bin->state;
				Item.stateMask = KQMOE_STATE_MASK;
				SendMessageA(List, LVM_SETITEMSTATE, Item.iItem, (LPARAM)&Item);
			}
			if (KQMOE_STATE_CHECKED == Bin->state) {
				Extract = 1;
			}
		}
		{
			RECT Rect;
			GetClientRect(List, &Rect);
			SendMessageA(List, LVM_SETCOLUMNWIDTH, 1, LVSCW_AUTOSIZE);
			SendMessageA(List, LVM_SETCOLUMNWIDTH, 0, Rect.right - Rect.left - (int)SendMessageA(List, LVM_GETCOLUMNWIDTH, 1, 0));
		}
		{
			LVITEMA Item = {LVIF_STATE, 0, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED, NULL, 0, 0, 0};
			SendMessageA(List, LVM_SETITEMSTATE, 0, (LPARAM)&Item);
		}
		if (Extract) {
			extract_hook(Dlg);
		}
		InterlockedCompareExchange(&s_list_init, 0, 1);
	}
}

static
INT_PTR dlg_list_changing(HWND Dlg, LPNMLISTVIEW Param)
{
	if ((0 == InterlockedCompareExchange(&s_list_init, 0, 0)) &&
	    ((LVIF_STATE & Param->uChanged) != 0) &&
	    ((LVIS_STATEIMAGEMASK & Param->uOldState) != 0) &&
	    ((LVIS_STATEIMAGEMASK & Param->uNewState) != 0) &&
	    ((LVIS_STATEIMAGEMASK & Param->uOldState) != (LVIS_STATEIMAGEMASK & Param->uNewState))) {
		BOOL Prevent = TRUE;
		LVITEMA Item = {LVIF_PARAM, 0, 0, 0, 0, NULL, 0, 0, 0};
		Item.iItem = Param->iItem;
		if (SendMessageA(Param->hdr.hwndFrom, LVM_GETITEMA, 0, (LPARAM)&Item)) {
			KQMOE_BIN *Bin = (KQMOE_BIN *)Item.lParam;
			if (Bin) {
				DWORD Status = write_bin(Bin, Param->uNewState & KQMOE_STATE_MASK);
				if (Status != ERROR_SUCCESS) {
					error_msg(Dlg, IDS_ERR_SET_IMPORT, MB_ICONERROR, Status);
				} else {
					if (KQMOE_STATE_CHECKED == (KQMOE_STATE_MASK & Param->uNewState)) {
						extract_hook(Dlg);
					}
					Prevent = FALSE;
				}
			}
		}
		SetWindowLongA(Dlg, DWL_MSGRESULT, Prevent);
		return (TRUE);
	}
	return (FALSE);
}

static
INT_PTR dlg_list_toggle(HWND List, int Index)
{
	LVITEMA Item = {LVIF_STATE, 0, 0, 0, LVIS_STATEIMAGEMASK, NULL, 0, 0, 0};
	Item.iItem = Index;
	Item.state = INDEXTOSTATEIMAGEMASK(1 | 2) & (UINT)SendMessageA(List, LVM_GETITEMSTATE, Index, LVIS_STATEIMAGEMASK);
	if (Item.state) {
		Item.state ^= INDEXTOSTATEIMAGEMASK(1 | 2);
		if (SendMessageA(List, LVM_SETITEMSTATE, Index, (LPARAM)&Item)) {
			return (TRUE);
		}
	}
	return (FALSE);
}

static
INT_PTR dlg_list_notify(HWND Dlg, LPNMHDR Param)
{
	switch (Param->code) {
	case LVN_ITEMCHANGING:
		return (dlg_list_changing(Dlg, (LPNMLISTVIEW)Param));
	case LVN_ITEMACTIVATE:
		return (dlg_list_toggle(Param->hwndFrom, ((LPNMLISTVIEW)Param)->iItem));
	// manual space/click handling for ancient Common Controls
	case LVN_KEYDOWN:
		if (!(LVS_EX_CHECKBOXES & dlg_list_exstyle) &&
		    (VK_SPACE == ((LPNMLVKEYDOWN)Param)->wVKey) &&
		    !(GetKeyState(VK_MENU) & 0x8000)) {
			int Index = (int)SendMessageA(Param->hwndFrom, LVM_GETNEXTITEM, (WPARAM)-1, LVIS_FOCUSED);
			if (Index >= 0) {
				return (dlg_list_toggle(Param->hwndFrom, Index));
			}
		}
		break;
	case NM_CLICK:
		if (!(LVS_EX_CHECKBOXES & dlg_list_exstyle)) {
			LVHITTESTINFO Info;
			DWORD Pos = GetMessagePos();
			Info.pt.x = GET_X_LPARAM(Pos);
			Info.pt.y = GET_Y_LPARAM(Pos);
			Info.flags = 0;
			Info.iItem = -1;
			MapWindowPoints(NULL, Param->hwndFrom, &Info.pt, 1);
			SendMessageA(Param->hwndFrom, LVM_HITTEST, 0, (LPARAM)&Info);
			// LVHT_ONITEMLABEL and LVHT_ONITEMICON are used to simulate LVS_EX_ONECLICKACTIVATE
			if ((Info.iItem >= 0) && ((LVHT_ONITEMSTATEICON | LVHT_ONITEMLABEL | LVHT_ONITEMICON) & Info.flags)) {
				return (dlg_list_toggle(Param->hwndFrom, Info.iItem));
			}
		}
		break;
	}
	return (FALSE);
}

static
void dlg_init_info(HWND Dlg)
{
	HWND edit = GetDlgItem(Dlg, IDC_INFO_TEXT);
	if (edit) {
		SendMessageA(edit, EM_SETEVENTMASK, 0, ENM_LINK | SendMessage(edit, EM_GETEVENTMASK, 0, 0));
		SendMessageA(edit, EM_AUTOURLDETECT, /*AURL_ENABLEEA*/1, 0);
		SendMessageA(edit, EM_SETBKGNDCOLOR, FALSE, (LPARAM)GetSysColor(COLOR_BTNFACE));
		{
			CHAR data[2048];
			if (LoadStringA(kqf_mod, IDS_INFO_TEXT, data, ARRAYSIZE(data)) > 0) {
				edit_stream_in(edit, data);
			}
		}
	}
}

static
INT_PTR dlg_info_notify(HWND Dlg, LPNMHDR Param)
{
	switch (Param->code) {
	case EN_LINK:
		{
			CHAR Url[2048 + 32 + sizeof("://")];
			ENLINK *Link = (ENLINK *)Param;
			if ((WM_LBUTTONUP == Link->msg) && (sizeof(Url) > Link->chrg.cpMax - Link->chrg.cpMin)) {
				TEXTRANGEA Range;
				Url[0] = '\0';
				Range.chrg.cpMin = Link->chrg.cpMin;
				Range.chrg.cpMax = Link->chrg.cpMax;
				Range.lpstrText = Url;
				SendMessageA(Link->nmhdr.hwndFrom, EM_GETTEXTRANGE, 0, (LPARAM)&Range);
				if (Url[0] != '\0') {
					if ((UINT_PTR)ShellExecuteA(Dlg, "open", Url, 0, 0, SW_SHOWNORMAL) > 32) {
						return (TRUE);
					}
				}
			}
		}
		break;
	}
	return (FALSE);
}

static
INT_PTR dlg_btn_click(HWND Dlg, WORD Id, HWND Wnd)
{
	switch (Id) {
	case IDCANCEL:
		EndDialog(Dlg, 0);
		return (TRUE);
	case IDC_SHIM_CBT:
	case IDC_SHIM_UNMAP:
	case IDC_SHIM_GDFS:
	case IDC_SHIM_GMEM:
	case IDC_SHIM_RMDIR:
	case IDC_SHIM_FIND:
	case IDC_CDROM_SIZE:
	case IDC_TALK_COMPLETE:
	case IDC_VIDEO_AVI:
	case IDC_VIDEO_NOBORDER:
	case IDC_VIDEO_NOAPPMOVE:
	case IDC_VIDEO_NOVIDMOVE:
	case IDC_GLIDE_NOWMSIZE:
	case IDC_GLIDE_DISABLE:
	case IDC_WINDOW_TITLE:
	case IDC_WINDOW_NOBORDER:
	case IDC_CDROM_FAKE:
	case IDC_MEM_TRACE:
	case IDC_TEXT_HEBREW_RTL:
		kqf_set_opt(Id - IDC_OPT_BASE, BST_CHECKED == SendMessageA(Wnd, BM_GETCHECK, 0, 0));
		kqf_save_cfg();
		return (TRUE);
	}
	return (FALSE);
}

static
INT_PTR dlg_sel_change(WORD Id, HWND Wnd)
{
	switch (Id) {
	case IDC_LOG_TYPE:
	case IDC_LOG_LEVEL:
	case IDC_MASK_DBG:
	case IDC_CRASH_DUMP:
		kqf_set_opt(Id - IDC_OPT_BASE, (int)SendMessageA(Wnd, CB_GETCURSEL, 0, 0));
		kqf_save_cfg();
		return (TRUE);
	}
	return (FALSE);
}

static
INT_PTR CALLBACK dlg_proc(HWND Dlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch(Msg) {
	case WM_INITDIALOG:
		dlg_init_title(Dlg);
		dlg_init_combo(GetDlgItem(Dlg, IDC_LOG_TYPE), IDS_LOG_TYPE_BASE, KQF_LOGT_COUNT, kqf_get_opt(KQF_CFGO_LOG_TYPE));
		dlg_init_combo(GetDlgItem(Dlg, IDC_LOG_LEVEL), IDS_LOG_LEVEL_BASE, KQF_LOGL_COUNT, kqf_get_opt(KQF_CFGO_LOG_LEVEL));
		dlg_init_combo(GetDlgItem(Dlg, IDC_MASK_DBG), IDS_MASK_DBG_BASE, KQF_OPT_MASK_DBG_COUNT, kqf_get_opt(KQF_CFGO_MASK_DBG));
		dlg_init_combo(GetDlgItem(Dlg, IDC_CRASH_DUMP), IDS_CRASH_DUMP_BASE, KQF_OPT_CRASH_DUMP_COUNT, kqf_get_opt(KQF_CFGO_CRASH_DUMP));
		dlg_init_check(Dlg);
		dlg_init_list(Dlg);
		dlg_init_info(Dlg);
		return (TRUE);
	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->idFrom) {
		case IDC_BIN_LIST:
			return (dlg_list_notify(Dlg, (LPNMHDR)lParam));
		case IDC_INFO_TEXT:
			return (dlg_info_notify(Dlg, (LPNMHDR)lParam));
		}
		break;
	case WM_COMMAND:
		switch (HIWORD(wParam)) {
		case BN_CLICKED:
			return (dlg_btn_click(Dlg, LOWORD(wParam), (HWND)lParam));
		case CBN_SELCHANGE:
			return (dlg_sel_change(LOWORD(wParam), (HWND)lParam));
		}
		break;
	case WM_SYSCOLORCHANGE:
		SendMessageA(GetDlgItem(Dlg, IDC_BIN_LIST), Msg, wParam, lParam);
		break;
	case WM_CLOSE:
		EndDialog(Dlg, 0);
		return (TRUE);
	}
	return (FALSE);
}

static
void show_help(DWORD std_handle)
{
	char hlp[1024];
	int len = LoadStringA(kqf_mod, IDS_HELP_TEXT, hlp, sizeof(hlp));
	HANDLE con = GetStdHandle(std_handle);
	if (0 >= len) {
		lstrcpynA(hlp, "Usage: kq8fix [--install]\n", sizeof(hlp));
		len = lstrlenA(hlp);
	}
	if ((NULL == con) || (INVALID_HANDLE_VALUE == con)) {
		// WinXP console: start /wait kq8fix --help
		BOOL (WINAPI *AttachConsole)(DWORD) =
			(BOOL (WINAPI *)(DWORD))GetProcAddress(
				GetModuleHandleA("KERNEL32.dll"), "AttachConsole");
		if ((AttachConsole != NULL) && AttachConsole(ATTACH_PARENT_PROCESS)) {
			con = GetStdHandle(std_handle);
		}
	}
	if ((con != NULL) && (con != INVALID_HANDLE_VALUE)) {
		if (CharToOemBuffA(hlp, hlp, len)) {
			DWORD dummy;
			if (GetConsoleMode(con, &dummy) && WriteConsoleA(con, hlp, len, &dummy, NULL)) {
				return;
			}
			if (WriteFile(con, hlp, len, &dummy, NULL)) {
				return;
			}
			OemToCharBuffA(hlp, hlp, len);
		}
	}
	MessageBoxA(NULL, hlp, "kqmoefix", (STD_ERROR_HANDLE == std_handle) ? MB_ICONQUESTION : MB_ICONASTERISK);
}


enum KQ8FIX_EXIT_CODE {
	KQ8FIX_EXIT_SUCCESS = 0,  // The operation completed successfully.
	KQ8FIX_EXIT_INVALID = 1,  // Invalid command.
	KQ8FIX_EXIT_NOKQMOE = 2,  // No game binary detected.
	KQ8FIX_EXIT_EXTRACT = 3,  // Failed to extract shim runtime library.
	KQ8FIX_EXIT_NOPATCH = 4   // Failed to modify a/the game binary.
};

int WINAPI WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CmdLine, int CmdShow)
{
	int result = KQ8FIX_EXIT_SUCCESS;
	BOOL com = SUCCEEDED(CoInitialize(NULL));
	LPSTR param = GetCommandLineA();
	UNREFERENCED_PARAMETER(Instance);
	UNREFERENCED_PARAMETER(PrevInstance);
	UNREFERENCED_PARAMETER(CmdLine);
	UNREFERENCED_PARAMETER(CmdShow);
	if ((param != NULL) && (*param != '\0')) {
	    // It's possible to create a process with a command-line that does
		// not start with the filename used to create the process (*.exe).
		// The code assumes that the filepath doesn't start with /? or --.
		if (((param[0] != '/') || (param[1] != '?')) &&
		    ((param[0] != '-') || (param[1] != '-'))) {
			if ('\"' == *param) {
				for (++param; *param != '\0'; ++param) {
					if ('\"' == *param) {
						++param;
						break;
					}
				}
			} else {
				while (*param > ' ') {
					++param;
				}
			}
			while ((*param != '\0') && (*param <= ' ')) {
				++param;
			}
		}
		{
			LPSTR back = &param[lstrlenA(param)];
			while ((--back >= param) && (*back <= ' ')) {
				*back = '\0';
			}
		}
	}
	InitCommonControls();
	{
		typedef BOOL (WINAPI *PFNINITCOMMONCONTROLSEX)(LPINITCOMMONCONTROLSEX);
		PFNINITCOMMONCONTROLSEX InitCommonControlsEx = (PFNINITCOMMONCONTROLSEX)
			GetProcAddress(GetModuleHandleA("COMCTL32"), "InitCommonControlsEx");
		if (InitCommonControlsEx) {
			INITCOMMONCONTROLSEX icc;
			icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
			icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES;
			InitCommonControlsEx(&icc);
		}
	}
	LoadLibraryA("RICHED32");
	LoadLibraryA("RICHED20");
	kqf_init();
	kqf_log(KQF_LOGL_NOTICE, "kq8fix: version %u.%u.%u.%u (%s)\n",
		KQF_VERF_MAJOR, KQF_VERF_MINOR, KQF_VERF_PATCH, KQF_VERF_FLAGS,
		KQF_VERS_UIVER);
	if ((param != NULL) && (*param != '\0')) {
		if ((0 == lstrcmpiA(param, "/?")) || (0 == lstrcmpiA(param, "--help"))) {

			show_help(STD_OUTPUT_HANDLE);

		} else if (0 == lstrcmpiA(param, "--extract")) {

			DWORD status = extract_hook(NULL);
			if (status != ERROR_SUCCESS) {
				kqf_log(KQF_LOGL_ERROR, "extract: failed to extract shim runtime (%#lx)\n", status);
				result = KQ8FIX_EXIT_EXTRACT;
			}
			kqf_save_cfg();
			{
				CHAR hlp[1024];
				int len = LoadStringA(kqf_mod, IDS_HELP_TEXT, hlp, ARRAYSIZE(hlp));
				if (len > 0) {
					HANDLE file;
					CHAR path[MAX_PATH];
					kqf_app_filepath("kq8fix.txt", path);
					file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
					if (file != INVALID_HANDLE_VALUE) {
						DWORD dummy;
						WriteFile(file, hlp, len * sizeof(hlp[0]), &dummy, NULL);
						CloseHandle(file);
					}
				}
			}

		} else if (0 == lstrcmpiA(param, "--install")) {

			KQMOE_BIN *bin = init_list();
			if (NULL == bin) {
				kqf_log(KQF_LOGL_ERROR, "install: no game binaries found\n");
				result = KQ8FIX_EXIT_NOKQMOE;
			} else {
				DWORD status = extract_hook(NULL);
				if (status != ERROR_SUCCESS) {
					kqf_log(KQF_LOGL_ERROR, "install: failed to extract shim runtime (%#lx)\n", status);
					status = KQ8FIX_EXIT_EXTRACT;
				}
				do {
					if (read_bin(bin) != KQMOE_STATE_CHECKED) {
						status = write_bin(bin, KQMOE_STATE_CHECKED);
						if (status != ERROR_SUCCESS) {
							kqf_log(KQF_LOGL_ERROR, "install: failed to patch '%s' (%#lx)\n", bin->name, status);
							result = KQ8FIX_EXIT_NOPATCH;
						} else {
							kqf_log(KQF_LOGL_INFO, "install: patched '%s'\n", bin->name);
						}
					}
					bin = bin->next;
				} while (bin != NULL);
			}

		} else {
			show_help(STD_ERROR_HANDLE);
			result = KQ8FIX_EXIT_INVALID;
		}
	} else {
		init_list();
		DialogBoxParamA(kqf_mod, MAKEINTRESOURCEA(IDD_MAIN), NULL, dlg_proc, 0);
	}
	if (com) {
		CoUninitialize();
	}
#ifdef _VC_NODEFAULTLIB
	ExitProcess(result);
#else
	return (result);
#endif
}

