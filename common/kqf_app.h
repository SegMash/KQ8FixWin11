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
#ifndef KQF_APP_H_
#define KQF_APP_H_

#include "kqf_win.h"

#pragma warning(push, 1)
# include <errno.h>
#pragma warning(pop)

#ifdef __cplusplus
extern "C" {
#endif


typedef enum KQMOE_VERSION_ {
	KQMOE_VERSION_10E,     // Retail (English)
	KQMOE_VERSION_10DEMO,  // Swamp Demo (English)
	KQMOE_VERSION_11FG,    // Retail (French, German)
	KQMOE_VERSION_11DEMO,  // Daventry Demo (English)
	KQMOE_VERSION_12E,     // Patch (English)
	KQMOE_VERSION_13BE,    // Retail/Patch (Brazilian Portuguese, English)
	KQMOE_VERSION_13FGIS,  // Retail/Patch (French, German, Italian, Spanish)
	KQMOE_VERSION_COUNT,
	KQMOE_VERSION_UNKNOWN = -1
} KQMOE_VERSION_;

typedef struct KQMOE_INFO {
	void const                    *base;  // HMODULE, IMAGE_DOS_HEADER *
	IMAGE_NT_HEADERS32 const      *header;
	IMAGE_SECTION_HEADER const    *sections;
	unsigned char const           *code_begin, *code_end;
	unsigned char const           *rdata_begin, *rdata_end;
	unsigned char                 *data_begin, *data_end;
	IMAGE_IMPORT_DESCRIPTOR const *imports;
	KQMOE_VERSION_                 version;
} KQMOE_INFO;

typedef struct KQF_APP {
	HINSTANCE  inst;
	int        path_len;
	CHAR       path[MAX_PATH];
	CHAR       name[MAX_PATH];
#ifdef KQF_RUNTIME
	KQMOE_INFO info;
#endif
} KQF_APP;

extern HINSTANCE kqf_mod /* = NULL */;  // instance of our own module
extern KQF_APP   kqf_app /* = {0}  */;  // application instance details
#ifdef KQF_RUNTIME
// between MSVCRT.__set_app_type(_GUI_APP) and DllMain(DLL_PROCESS_DETACH)
extern LONG /*volatile*/ runtime_active /* = 0 */;
#endif


// initializes kqf_mod and kqf_app
void kqf_init_app(void);  // called by kqf_init


// relative to absolute application filename
void kqf_app_filepath(char const *name, char path[MAX_PATH]);


// get info for module/app instance or mapped file
int kqmoe_info(KQMOE_INFO *info, void const *base);
void const *kqmoe_rva_ptr(KQMOE_INFO const *info, DWORD rva);
void const *kqmoe_va_ptr(KQMOE_INFO const *info, DWORD va);


#define KQMOE_RT_MSVCRT "MSVCRT.dll"
#define KQMOE_RT_FIXOLD "maskrt.dll"
#define KQMOE_RT_FIXNEW "kq8fix.dll"
// get a pointer to the runtime import filename
char const *kqmoe_rt(KQMOE_INFO const *info);


#ifdef __cplusplus
}
#endif
#endif
