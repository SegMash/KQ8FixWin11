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
#include "hook_gfx.h"

#include "../common/kqf_app.h"
#include "../common/kqf_cfg.h"
#include "../common/kqf_log.h"


typedef
struct GFXSurface GFXSurface;
struct GFXSurface {
	void *const *__vfptr;  // GFXSurface::`vftable' const*
	void       **Table;    // FunctionTable*
	DWORD        Flags;
	RECT         ClipRect;
	RECT         LastRect;
	float        Gamma;
	void        *Palette;  // GFXPalette*
	void        *Surface;
	LONG         Pitch;
	LONG         Width;
	LONG         Height;
	LONG         BitCount;
	GFXSurface  *Next;
	LONG         Count;
	BOOL         IsPrimary;
	int          Type;
	LONG         PopCount;
	LONG         LockCount;
	LONG         __unknown[2];
	HDC          Context;
};


static void (__fastcall **func_GFXClearScreen)(GFXSurface *surface, DWORD color) /* = NULL */;
static void (__fastcall  *mask_GFXClearScreen)(GFXSurface *surface, DWORD color) /* = NULL */;
static void  __fastcall   shim_GFXClearScreen (GFXSurface *surface, DWORD color)
{
	if (!surface) {
		kqf_log(KQF_LOGL_INFO, "GFXClearScreen: invalid surface parameter.\n");
	}
	else if (!surface->Surface) {
		kqf_log(KQF_LOGL_INFO, "GFXClearScreen: invalid surface buffer (lock count: %li).\n", surface->LockCount);
	} else {
		mask_GFXClearScreen(surface, color);
	}
}


HMODULE WINAPI KERNEL32_LoadLibraryA(LPCSTR lpLibFileName)
{
	HMODULE result;
	KQF_TRACE("LoadLibraryA<%#08lx>('%s')\n", ReturnAddress, lpLibFileName);
	if (!IsBadReadPtr(lpLibFileName, sizeof("glide2x.dll")) && (0 == lstrcmpiA(lpLibFileName, "glide2x.dll"))) {
		if (kqf_get_opt(KQF_CFGO_GLIDE_DISABLE)) {
			kqf_log(KQF_LOGL_INFO, "LoadLibraryA: skipped glide2x.dll loading\n");
			SetLastError(ERROR_FILE_NOT_FOUND);
			result = NULL;
		} else {
			result = LoadLibraryA(lpLibFileName);
			// enable nGlide developer log if tracing (version 0.93 - 1.05)
			if (result && (kqf_get_log_level() >= KQF_LOGL_TRACE)) {
				DWORD ErrCode = GetLastError();
				DWORD ErrMode = SetErrorMode(SEM_NOOPENFILEERRORBOX); SetErrorMode(SEM_NOOPENFILEERRORBOX | ErrMode);
				{
					DWORD Attributes = GetFileAttributesA("E:\\glide\\nglide\\logs");  // this directory has to exist on your machine
					if ((FILE_ATTRIBUTE_DIRECTORY & Attributes) && (Attributes != INVALID_FILE_ATTRIBUTES)) {
						IMAGE_DOS_HEADER const *const DosHeader = (IMAGE_DOS_HEADER const *)result;
						IMAGE_NT_HEADERS32 const *const NtHeaders = (IMAGE_NT_HEADERS32 const *)((LPBYTE)result + DosHeader->e_lfanew);
						if ((DosHeader->e_magic == IMAGE_DOS_SIGNATURE) && (DosHeader->e_lfanew > 0) &&
						    (NtHeaders->Signature == IMAGE_NT_SIGNATURE) && (NtHeaders->FileHeader.NumberOfSections > 0)) {
							IMAGE_SECTION_HEADER const *const Section = IMAGE_FIRST_SECTION(NtHeaders);
							if (Section->SizeOfRawData && Section->Misc.VirtualSize &&
							    (0 == lstrcmpA(".text", (LPCSTR)(&Section->Name[0])))) {
								BYTE const *SectionPos;
								BYTE const *const SectionStart = (LPBYTE)result + Section->VirtualAddress;
								BYTE const *const SectionEnd = SectionStart + Section->Misc.VirtualSize - 0x00000020;
								for (SectionPos = SectionStart; SectionPos < SectionEnd; SectionPos += 0x00000010) {
									if (0x0824448B == *(DWORD const *)SectionPos) {
										BYTE const *Code = SectionPos + 4;
										if (0x7400E883 == *(DWORD const *)Code) {
											Code += 5;  // version 1.02+
										}
										if (0xA1007548 == (0xFF00FFFF & *(DWORD const *)Code)) {
											BOOL *const NGlideLogEnabled = *(BOOL *const *)(Code + 4);
											Code += 8;
											if ((0x0B76C085 == *(DWORD const *)Code) &&
											    (0x68 == Code[4])) {
												char const *const NGlideLogFileName = *(char const *const *)(Code + 5);
												Code += 9;
												if (0x15FF == *(WORD const *)Code) {
													KQF_TRACE("Glide: devlog flag %#08lx name %#08lx\n",
														(DWORD_PTR)NGlideLogEnabled - (DWORD_PTR)result + NtHeaders->OptionalHeader.ImageBase,
														(DWORD_PTR)NGlideLogFileName - (DWORD_PTR)result + NtHeaders->OptionalHeader.ImageBase);
													if ((0 == *NGlideLogEnabled) &&
													    (0 == lstrcmpiA(NGlideLogFileName, "E:\\glide\\nglide\\logs\\log.wri"))) {
														*NGlideLogEnabled = 1;
														kqf_log(KQF_LOGL_INFO, "Glide: enabled nGlide developer log\n");
													}
													break;
												}
											}
										}
									}
								}
							}
						}
					}
				}
				SetErrorMode(ErrMode);
				SetLastError(ErrCode);
			}
		}
	} else {
		result = LoadLibraryA(lpLibFileName);
	}
	KQF_TRACE("LoadLibraryA<%#08lx>('%s')[%#08lx]{%#lx}\n", ReturnAddress, lpLibFileName, result, result != NULL ? ERROR_SUCCESS : GetLastError());
	return (result);
}


void hook_GFXClearScreen(void)
{
	//KQF_TRACE("GFXClearScreen: hooking\n");
	if (func_GFXClearScreen && (shim_GFXClearScreen == *func_GFXClearScreen))
		return;
	if (!kqf_app.info.code_begin || !kqf_app.info.data_begin ||
	    (kqf_app.info.code_end - kqf_app.info.code_begin < 0x0060) ||
	    (kqf_app.info.data_end - kqf_app.info.data_begin < 0x00D8)) {
		kqf_log(KQF_LOGL_ERROR, "GFXClearScreen: invalid code and/or data section.\n");
	} else {
		// scan for 'flushCache',0, 0, 'outline',0, 0,0,0,0, <RasterClipTable>
		DWORD *data;
		DWORD *const data_begin = (DWORD *)kqf_app.info.data_begin;
		DWORD *const data_end = (DWORD *)(kqf_app.info.data_end - 0x00D8);
		for (data = data_begin; data < data_end; ++data) {
			if ((0x73756C66 == data[0]) &&
			    (0x63614368 == data[1]) &&
			    (0x00006568 == data[2]) &&
			    (0x6C74756F == data[3]) &&
			    (0x00656E69 == data[4]) &&
			    (0x00000000 == data[5])) {
				DWORD i;
				DWORD const code_begin = (DWORD)(DWORD_PTR)kqf_app.info.code_begin;
				DWORD const code_end = (DWORD)(DWORD_PTR)kqf_app.info.code_end;
				for (i = 6 + 0; i < 6 + 48; ++i) {
					if ((data[i] < code_begin) || (code_end <= data[i])) {
						kqf_log(KQF_LOGL_WARNING, "GFXClearScreen: invalid function table entry.\n");
						return;
					}
				}
				func_GFXClearScreen = (void (__fastcall **)(GFXSurface *, DWORD))&data[6 + 0];
				mask_GFXClearScreen = *func_GFXClearScreen;
				*func_GFXClearScreen = shim_GFXClearScreen;
				kqf_log(KQF_LOGL_INFO, "GFXClearScreen: found and hooked at %#08lx.\n", mask_GFXClearScreen);
				return;
			}
		}
		kqf_log(KQF_LOGL_WARNING, "GFXClearScreen: pattern not found.\n");
	}
}

void unhook_GFXClearScreen(void)
{
	if (func_GFXClearScreen && mask_GFXClearScreen && (shim_GFXClearScreen == *func_GFXClearScreen))
		*func_GFXClearScreen = mask_GFXClearScreen;
}


void patch_D3DTotalVideoMemory(void)
{
	//KQF_TRACE("D3DTotalVideoMemory: patching\n");
	if (!kqf_app.info.code_begin ||
	    (kqf_app.info.code_end - kqf_app.info.code_begin < 0x001C)) {
		kqf_log(KQF_LOGL_ERROR, "D3DTotalVideoMemory: invalid code section.\n");
	} else {
		// 8B 0D __ __ __ __               mov     ecx, Direct3D::DeviceResolutions.array
		// 8B 14 B1                        mov     edx, [ecx+esi*4]
		// 83 3A 00                        cmp     dword ptr [edx], 00h
		// 75 0A                           jnz     short $+0Ch
		// 33 C0                           xor     eax, eax
		// 5D                              pop     ebp
		// 5F                              pop     edi
		// 5E                              pop     esi
		// 5B                              pop     ebx
		// 83 C4 10                        add     esp, 10h
		// C3                              retn
		// C7 44 24 18 00 00 00 00         mov     dword ptr [esp+18h], 00000000h
		// 8B CF                           mov     ecx, edi ; -> mov     ecx, esi
		// E8 __ __ __ __                  call    Direct3D::GetTotalVideoMemory
		DWORD *code;
		DWORD *const code_begin = (DWORD *)kqf_app.info.code_begin;
		DWORD *const code_end = (DWORD *)(kqf_app.info.code_end - 0x001C);
		for (code = code_begin; code < code_end; ++code) {
			if ((0x3A83B114 == code[0]) &&
			    (0x330A7500 == code[1]) &&
			    (0x5E5F5DC0 == code[2]) &&
			    (0x10C4835B == code[3]) &&
			    (0x2444C7C3 == code[4]) &&
			    (0x00000018 == code[5])) {
				switch (code[6]) {
				case 0xE8CF8B00:
					{
						MEMORY_BASIC_INFORMATION mem;
						DWORD read_only;
						if (!kqf_query_mem(code, mem))
							mem.Protect = PAGE_NOACCESS;
						read_only = mem.Protect & (PAGE_NOACCESS | PAGE_READONLY | PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_GUARD);
						if (read_only && !VirtualProtect(code, 0x001C, PAGE_EXECUTE_READWRITE, &mem.Protect)) {
							kqf_log(KQF_LOGL_ERROR, "D3DTotalVideoMemory: failed to change memory protection (%#lx).\n", GetLastError());
						} else {
							code[6] = 0xE8F18900;
							if (read_only && (mem.Protect != PAGE_NOACCESS))
								VirtualProtect(code, 0x001C, mem.Protect, &mem.Protect);
							FlushInstructionCache(GetCurrentProcess(), code, 0x001C);
							kqf_log(KQF_LOGL_INFO, "D3DTotalVideoMemory: found and patched at %#08lx.\n", (BYTE *)code + 0x0019);
						}
					}
					return;
				case 0xE8F18900:
					kqf_log(KQF_LOGL_INFO, "D3DTotalVideoMemory: function already patched.\n");
					return;
				}
				break;
			}
		}
		kqf_log(KQF_LOGL_WARNING, "D3DTotalVideoMemory: pattern not found.\n");
	}
}


void patch_BrightnessSlider(void)
{
	//KQF_TRACE("BrightnessSlider: patching\n");
	if (!kqf_app.info.rdata_begin ||
	    (kqf_app.info.rdata_end - kqf_app.info.rdata_begin < 0x0010)) {
		kqf_log(KQF_LOGL_ERROR, "BrightnessSlider: invalid read-only data section.\n");
	} else {
		// 0x42C80000 (100.0)
		// 0x3C23D70A (0.01)
		// 0x3E99999A (0.3)
		// 0x428EDB6D (71.428566) -> 0x428F9249 (71.78571)
		DWORD *rdata;
		DWORD *const rdata_begin = (DWORD *)kqf_app.info.rdata_begin;
		DWORD *const rdata_end = (DWORD *)(kqf_app.info.rdata_end - 0x0010);
		for (rdata = rdata_begin; rdata < rdata_end; ++rdata) {
			if ((0x42C80000 == rdata[0]) &&
			    (0x3C23D70A == rdata[1]) &&
			    (0x3E99999A == rdata[2])) {
				switch (rdata[3]) {
				case 0x428EDB6D:
					{
						MEMORY_BASIC_INFORMATION mem;
						DWORD read_only;
						if (!kqf_query_mem(rdata, mem))
							mem.Protect = PAGE_NOACCESS;
						read_only = mem.Protect & (PAGE_NOACCESS | PAGE_READONLY | PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_GUARD);
						if (read_only && !VirtualProtect(rdata, 0x0010, PAGE_READWRITE, &mem.Protect)) {
							kqf_log(KQF_LOGL_ERROR, "BrightnessSlider: failed to change memory protection (%#lx).\n", GetLastError());
						} else {
							rdata[3] = 0x428F9249;
							if (read_only && (mem.Protect != PAGE_NOACCESS))
								VirtualProtect(rdata, 0x0010, mem.Protect, &mem.Protect);
							kqf_log(KQF_LOGL_INFO, "BrightnessSlider: found and patched at %#08lx.\n", &rdata[3]);
						}
					}
					return;
				case 0x428F9249:
					kqf_log(KQF_LOGL_INFO, "BrightnessSlider: constant already patched.\n");
					return;
				}
				break;
			}
		}
		kqf_log(KQF_LOGL_WARNING, "BrightnessSlider: pattern not found.\n");
	}
}
