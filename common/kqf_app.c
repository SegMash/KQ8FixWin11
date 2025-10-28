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
#include "kqf_app.h"


static
int init_info(KQMOE_INFO *info)
{
	if (!info->base) {
		return (0);
	}
	{
		IMAGE_DOS_HEADER const *mz = (IMAGE_DOS_HEADER const *)info->base;
		IMAGE_NT_HEADERS32 const *pe = (IMAGE_NT_HEADERS32 const *)((unsigned char const *)info->base + mz->e_lfanew);
		if ((mz->e_magic != IMAGE_DOS_SIGNATURE) ||
		    (0 >= mz->e_lfanew) ||
		    (pe->Signature != IMAGE_NT_SIGNATURE) ||
		    (0 >= pe->FileHeader.NumberOfSections))
			return (0);
		info->header = pe;
		info->sections = IMAGE_FIRST_SECTION(info->header);
	}
	{
		IMAGE_NT_HEADERS32 const *hdr = info->header;
		IMAGE_OPTIONAL_HEADER32 const *opt = &(hdr->OptionalHeader);
		IMAGE_DATA_DIRECTORY const *imp = &(opt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]);
		if ((hdr->FileHeader.SizeOfOptionalHeader < (WORD)((unsigned char const *)(imp + 1) - (unsigned char const *)opt)) ||
		    (opt->Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) ||
		    (opt->NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_IMPORT) ||
		    (0 == imp->VirtualAddress) ||
		    (imp->Size < sizeof(IMAGE_IMPORT_DESCRIPTOR))) {
			return (0);
		}
		info->imports = kqmoe_rva_ptr(info, imp->VirtualAddress);
	}
	{
		WORD i;
		for (i = 0; i < info->header->FileHeader.NumberOfSections; ++i) {
			IMAGE_SECTION_HEADER const *const section = &info->sections[i];
			if (NULL == info->code_begin) {
				if (((IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE) ==
				    ((IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_EXECUTE) & section->Characteristics)) &&
				    (0 == lstrcmpA((LPCSTR)&section->Name[0], ".text"))) {
					info->code_begin = kqmoe_rva_ptr(info, section->VirtualAddress);
					info->code_end = info->code_begin + ((
						section->Misc.VirtualSize < section->SizeOfRawData) ?
						section->Misc.VirtualSize : section->SizeOfRawData);
				}
			} else if (NULL == info->rdata_begin) {
				if (((IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ) ==
				    ((IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ) & section->Characteristics)) &&
				    (0 == lstrcmpA((LPCSTR)&section->Name[0], ".rdata"))) {
					info->rdata_begin = kqmoe_rva_ptr(info, section->VirtualAddress);
					info->rdata_end = info->rdata_begin + ((
						section->Misc.VirtualSize < section->SizeOfRawData) ?
						section->Misc.VirtualSize : section->SizeOfRawData);
				}
			} else if (NULL == info->data_begin) {
				if (((IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE) ==
				    ((IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE) & section->Characteristics)) &&
				    (0 == lstrcmpA((LPCSTR)&section->Name[0], ".data"))) {
					info->data_begin = (unsigned char *)kqmoe_rva_ptr(info, section->VirtualAddress);
					info->data_end = info->data_begin + ((
						section->Misc.VirtualSize < section->SizeOfRawData) ?
						section->Misc.VirtualSize : section->SizeOfRawData);
				}
			} else {
				break;
			}
		}
	}
	return (1);
}


static
struct MONSTER_RVA {
	DWORD td;  // RTTI Type Descriptor
	DWORD ol;  // RTTI Complete Object Locator
	DWORD vf;  // vftable
} const monster_rva[KQMOE_VERSION_COUNT] = {
	{0x001AD298, 0x0018CCC0, 0x0017F2DC},  // KQMOE_VERSION_10E
	{0x001ADE10, 0x0018E280, 0x0018024C},  // KQMOE_VERSION_10DEMO
	{0x001AD620, 0x0018ED48, 0x00180394},  // KQMOE_VERSION_11FG
	{0x001AD298, 0x0018CCC8, 0x0017F2DC},  // KQMOE_VERSION_11DEMO
	{0x001ADE38, 0x0018E288, 0x00180254},  // KQMOE_VERSION_12E
	{0x001ACE28, 0x0018ECF0, 0x00180BB4},  // KQMOE_VERSION_13BE
	{0x001AE620, 0x0018FD58, 0x0018139C}   // KQMOE_VERSION_13FGIS
};

static
int detect_version(KQMOE_INFO *info)
{
	int version;
	for (version = KQMOE_VERSION_COUNT - 1; version != KQMOE_VERSION_UNKNOWN; --version) {
		struct TypeDescriptor {
			DWORD hash;
			DWORD spare;
			CHAR  name[sizeof(".?AVKQMonster@@")];
		} const *td = kqmoe_rva_ptr(info, monster_rva[version].td);
		if ((td != NULL) && !IsBadReadPtr(td, sizeof(*td)) && (0 == td->spare) &&
		    (0 == lstrcmpA(td->name, ".?AVKQMonster@@"))) {
			struct RTTICompleteObjectLocator {
				DWORD signature;
				DWORD offset;
				DWORD cdOffset;
				DWORD pTypeDescriptor;
				DWORD pClassDescriptor;
			} const *ol = kqmoe_rva_ptr(info, monster_rva[version].ol);
			if ((ol != NULL) && !IsBadReadPtr(ol, sizeof(*ol)) && (0 == ol->signature) && (0 == ol->offset) && (0 == ol->cdOffset)) {
				if (td == kqmoe_va_ptr(info, ol->pTypeDescriptor)) {
					DWORD const *vf = kqmoe_rva_ptr(info, monster_rva[version].vf);
					if ((vf != NULL) && !IsBadReadPtr(&vf[-1], sizeof(*vf)) &&
					    (ol == kqmoe_va_ptr(info, vf[-1]))) {
						info->version = version;
						return (1);
					}
				}
			}
		}
	}
	return (0);
}


HINSTANCE kqf_mod /* = NULL */;
KQF_APP   kqf_app /* = {0}  */;
#ifdef KQF_RUNTIME
LONG /*volatile*/ runtime_active /* = 0 */;
#endif


void kqf_init_app(void)
{
	if (NULL == kqf_mod) {
		MEMORY_BASIC_INFORMATION mbi;
		if (kqf_query_mem((LPCVOID)(ULONG_PTR)kqf_init_app, mbi)) {
			kqf_mod = (HINSTANCE)mbi.AllocationBase;
		}
	}
	if (NULL == kqf_app.inst) {
		CHAR filename[MAX_PATH];
		DWORD size = GetModuleFileNameA(NULL, filename, ARRAYSIZE(filename));
		if ((0 < size) && (size < ARRAYSIZE(filename))) {
			CHAR fullname[MAX_PATH];
			LPSTR filepart = NULL;
			size = GetFullPathNameA(filename, ARRAYSIZE(fullname), fullname, &filepart);
			if ((0 < size) && (size < ARRAYSIZE(fullname)) && (filepart != NULL) && (filepart[0] != '\0')) {
				lstrcpynA(kqf_app.name, filepart, ARRAYSIZE(kqf_app.name));
				*filepart = '\0';
				lstrcpynA(kqf_app.path, fullname, ARRAYSIZE(kqf_app.path));
			}
		}
		if ('\0' == kqf_app.path[0]) {
			lstrcpyA(kqf_app.path, ".\\");
		}
		kqf_app.path_len = lstrlenA(kqf_app.path);
		kqf_app.inst = (HINSTANCE)GetModuleHandleA(NULL);
#ifdef KQF_RUNTIME
		kqmoe_info(&kqf_app.info, kqf_app.inst);
#endif
	}
}


void kqf_app_filepath(char const *name, char path[MAX_PATH])
{
	if (NULL == kqf_app.inst) {
		kqf_init_app();
	}
	if ((0 == kqf_app.path_len) || (NULL == lstrcpynA(path, kqf_app.path, MAX_PATH))) {
		lstrcpyA(path, ".\\");
		if (NULL == lstrcpynA(&path[2], name, MAX_PATH - 2)) {
			if (NULL == lstrcpynA(path, name, MAX_PATH)) {
				path[MAX_PATH - 1] = '\0';
			}
		}
	} else if (NULL == lstrcpynA(&path[kqf_app.path_len], name, MAX_PATH - kqf_app.path_len)) {
		path[MAX_PATH - 1] = '\0';
	}
}


int kqmoe_info(KQMOE_INFO *info, void const *base)
{
	info->base        = base;
	info->header      = NULL;
	info->sections    = NULL;
	info->code_begin  = NULL;
	info->code_end    = NULL;
	info->rdata_begin = NULL;
	info->rdata_end   = NULL;
	info->data_begin  = NULL;
	info->data_end    = NULL;
	info->imports     = NULL;
	info->version     = KQMOE_VERSION_UNKNOWN;
	if (!init_info(info)) {
		return (0);
	}
	return (detect_version(info) + 1);
}

#if defined(KQF_RUNTIME)

void const *kqmoe_rva_ptr(KQMOE_INFO const *info, DWORD rva)
{
	if (rva > info->header->OptionalHeader.SizeOfImage) {
		return (NULL);
	}
	return ((unsigned char const *)info->base + rva);
}

void const *kqmoe_va_ptr(KQMOE_INFO const *info, DWORD va)
{
	if (((unsigned char const *)va < (unsigned char const *)info->base) ||
	    ((unsigned char const *)va >= (unsigned char const *)info->base + info->header->OptionalHeader.SizeOfImage)) {
		return (NULL);
	}
	return ((void const *)va);
}

#elif defined(KQF_SETUP)

void const *kqmoe_rva_ptr(KQMOE_INFO const *info, DWORD rva)
{
	WORD sec;
	if ((NULL == info) ||
	    (NULL == info->base) ||
	    (NULL == info->header) ||
	    (NULL == info->sections) ||
	    (rva >= info->header->OptionalHeader.SizeOfImage))
		return (NULL);
	for (sec = 0; sec < info->header->FileHeader.NumberOfSections; ++sec) {
		IMAGE_SECTION_HEADER const *const section = &(info->sections[sec]);
		if ((section->VirtualAddress <= rva) && (rva < section->VirtualAddress + section->Misc.VirtualSize)) {
			DWORD const offset = rva - section->VirtualAddress;
			if (offset < section->SizeOfRawData)
				return ((unsigned char const *)info->base + section->PointerToRawData + offset);
			return (NULL);
		}
	}
	return (NULL);
}

void const *kqmoe_va_ptr(KQMOE_INFO const *info, DWORD va)
{
	if ((NULL == info) ||
	    (NULL == info->header) ||
	    (va < info->header->OptionalHeader.ImageBase))
		return (NULL);
	return (kqmoe_rva_ptr(info, va - info->header->OptionalHeader.ImageBase));
}

#endif


char const * kqmoe_rt(KQMOE_INFO const *info)
{
	IMAGE_IMPORT_DESCRIPTOR const *import;
	if (!info || !info->imports)
		return (NULL);
	for (import = info->imports; import->Name != 0; ++import) {
		char const *name = kqmoe_rva_ptr(info, import->Name);
		if (name && (
		    (0 == lstrcmpiA(name, KQMOE_RT_MSVCRT)) || (0 == lstrcmpiA(name, KQMOE_RT_FIXOLD)) ||
		    (0 == lstrcmpiA(name, KQMOE_RT_FIXNEW)))) {
			return (name);
		}
	}
	return (NULL);
}

