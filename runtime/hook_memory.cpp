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

#include "../common/kqf_app.h"
#include "../common/kqf_cfg.h"
#include "../common/kqf_log.h"
#include <intrin.h>

#pragma intrinsic(_ReturnAddress)

#pragma warning(disable: 4483)  // expected C++ keyword
#pragma warning(disable: 4514)  // unreferenced inline function has been removed

// Visual Studio intrinsic for return address
#define ReturnAddress _ReturnAddress()

// Disable the C++ standard compliance check for operator new/delete in namespace
#pragma warning(disable: 4595)  // illegal inline operator: operator can't be declared 'inline'

extern "C" extern
void *(__cdecl *__identifier("_imp_??2@YAPAXI@Z"))(unsigned int size);

extern "C" extern
void (__cdecl *__identifier("_imp_??3@YAXPAX@Z"))(void *ptr);

// Use the old namespace approach with a workaround for modern compilers
// We'll define these as extern "C" functions with C++ mangled names
extern "C" {
	// Export with the exact mangled names that the linker expects
	__declspec(dllexport) void * __cdecl __identifier("??2MSVCRT@@YAPAXI@Z")(unsigned int size)
	{
		void *result = __identifier("_imp_??2@YAPAXI@Z")(size);
		if (runtime_active) {
			if (NULL == result) {
				kqf_log(KQF_LOGL_ERROR, "operator new: failed to allocate %u bytes at %#08lx.\n", size, ReturnAddress);
			} 
		}
		return (result);
	}

	__declspec(dllexport) void __cdecl __identifier("??3MSVCRT@@YAXPAX@Z")(void *ptr)
	{
		if (ptr != NULL) {
			__identifier("_imp_??3@YAXPAX@Z")(ptr);
		}
	}
}

