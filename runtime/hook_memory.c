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
#include "hook_memory.h"

#include "../common/kqf_app.h"
#include "../common/kqf_cfg.h"
#include "../common/kqf_log.h"
#include "../common/kqf_win.h"


extern
void *(__cdecl *_imp__malloc)(unsigned int size);
void * __cdecl MSVCRT_malloc (unsigned int size)
{
	void *result = _imp__malloc(size);
	if (runtime_active) {
		if (NULL == result) {
			kqf_log(KQF_LOGL_ERROR, "malloc: failed to allocate %u bytes at %#08lx.\n", size, ReturnAddress);
		} else if (kqf_get_opt(KQF_CFGO_MEM_TRACE)) {
			kqf_log(KQF_LOGL_FORCE, "malloc<%#08lx>(%u)[%#08lx]\n", ReturnAddress, size, result);
		}
	}
	return (result);
}

extern
void *(__cdecl *_imp__realloc)(void *ptr, unsigned int size);
void * __cdecl MSVCRT_realloc (void *ptr, unsigned int size)
{
	void *result = _imp__realloc(ptr, size);
	if (runtime_active) {
		if (NULL == result) {
			kqf_log(KQF_LOGL_ERROR, "realloc: failed to allocate %u bytes for %#08lx at %#08lx.\n", size, ptr, ReturnAddress);
		} else if (kqf_get_opt(KQF_CFGO_MEM_TRACE)) {
			kqf_log(KQF_LOGL_FORCE, "realloc<%#08lx>(%#08lx,%u)[%#08lx]\n", ReturnAddress, ptr, size, result);
		}
	}
	return (result);
}

extern
void (__cdecl *_imp__free)(void *ptr);
void  __cdecl MSVCRT_free (void *ptr)
{
	if (ptr != NULL) {
		if (runtime_active && kqf_get_opt(KQF_CFGO_MEM_TRACE)) {
			kqf_log(KQF_LOGL_FORCE, "free<%#08lx>(%#08lx)\n", ReturnAddress, ptr);
		}
		_imp__free(ptr);
	}
}

