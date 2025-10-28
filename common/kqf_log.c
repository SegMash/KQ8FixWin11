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
#include "kqf_log.h"

#include "kqf_app.h"
#include "kqf_cfg.h"
#include "kqf_win.h"

#pragma warning(push, 1)
# include <stdio.h>
# include <stdarg.h>
# include <varargs.h>
#pragma warning(pop)


enum LOGINIT_ {
	LOGINIT_NONE = 2,
	LOGINIT_INIT = 1,
	LOGINIT_DONE = 0
};
enum LOGLINE_ {
	LOGLINE_SIZE = 1024  // maximum buffer size of wvsprintf
};


static LONG /*volatile*/ s_init = LOGINIT_NONE;
static LONG /*volatile*/ s_type = KQF_LOGT_DEFAULT;
static LONG /*volatile*/ s_level = KQF_LOGL_DEFAULT;
static HANDLE s_file /* = NULL */;
static CRITICAL_SECTION s_lock /* = {0} */;


static
void init(void)
{
	switch (InterlockedCompareExchange(&s_init, LOGINIT_INIT, LOGINIT_NONE)) {
	case LOGINIT_NONE:
		InitializeCriticalSection(&s_lock);
		{
			BOOL (WINAPI *SetCriticalSectionSpinCount)(LPCRITICAL_SECTION, DWORD) =
				(BOOL (WINAPI *)(LPCRITICAL_SECTION, DWORD))GetProcAddress(
					GetModuleHandleA("KERNEL32"), "SetCriticalSectionSpinCount");
			if (SetCriticalSectionSpinCount != NULL)
				SetCriticalSectionSpinCount(&s_lock, 4000);
		}
		InterlockedCompareExchange(&s_init, LOGINIT_DONE, LOGINIT_INIT);
		break;
	case LOGINIT_INIT:
		while (LOGINIT_INIT == InterlockedCompareExchange(&s_init, LOGINIT_DONE, LOGINIT_DONE))
			;
		break;
	}
}


static
int print_line(char line[LOGLINE_SIZE], char const *format, va_list args)
{
	int size = wvsprintfA(line, format, args);
	if (size < 0) {
		size = 0;
	} else if (size >= LOGLINE_SIZE) {
		size = LOGLINE_SIZE - 1;
	}
	line[size] = '\0';
	return (size);
};


static
int file_valid(void)
{
	if ((NULL == s_file) || (INVALID_HANDLE_VALUE == s_file)) {
		return (0);
	}
	return (1);
}

#ifdef KQF_SETUP
# define KQF_LOG_SUFFIX ".log"
#else
# define KQF_LOG_SUFFIX ".kq8fix.log"
#endif

static
int create_file(void)
{
	if (!file_valid()) {
		CHAR name[MAX_PATH];
		DWORD size = GetModuleFileNameA(NULL, name, ARRAYSIZE(name));
		if ((0 < size) && (size < ARRAYSIZE(name))) {
			LPSTR ext = &name[size];
			while (--ext > name) {
				if ('.' == *ext) {
					break;
				}
				if (('\\' == *ext) || ('/' == *ext) || (':' == *ext)) {
					ext = &name[size];
					break;
				}
			}
			if (ext - name + sizeof(KQF_LOG_SUFFIX) <= ARRAYSIZE(name)) {
				if (lstrcpyA(ext, KQF_LOG_SUFFIX) != NULL) {
					s_file = CreateFileA(name, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
					return (file_valid());
				}
			}
		}
	}
	return (0);
}

static
void close_file(void)
{
	if (file_valid()) {
		FlushFileBuffers(s_file);
		CloseHandle(s_file), s_file = NULL;
	}
}


static
void log_null(char const *format, va_list args)
{
	UNREFERENCED_PARAMETER(format);
	UNREFERENCED_PARAMETER(args);
}

static
void log_file(char const *format, va_list args)
{
	if (!file_valid())
		if (!create_file())
			return;
	{
		DWORD dummy;
		char line[LOGLINE_SIZE];
		char const *text = (args != NULL) ? ((print_line(line, format, args) > 0) ? line : format) : format;
		WriteFile(s_file, text, lstrlenA(text), &dummy, NULL);
	}
}

static
void log_ods(char const *format, va_list args)
{
	char text[sizeof("[kq8fix] ") - 1 + LOGLINE_SIZE];
	char *line = &text[sizeof("[kq8fix] ") - 1];
	if (NULL == lstrcpyA(text, "[kq8fix] ")) {
		return;
	}
	if ((NULL == args) || (0 >= print_line(line, format, args))) {
		if (NULL == lstrcpynA(line, format, LOGLINE_SIZE)) {
			return;
		}
	}
	OutputDebugStringA(text);
}

static
void log_both(char const *format, va_list args)
{
	log_file(format, args);
	log_ods(format, args);
}

static
void (*const c_func[KQF_LOGT_COUNT])(char const *, va_list) = {
	log_null,  // KQF_LOGT_NULL
	log_file,  // KQF_LOGT_FILE
	log_ods,   // KQF_LOGT_ODS
	log_both   // KQF_LOGT_BOTH
};


void kqf_init_log(void)
{
	init();
	kqf_set_log_type(kqf_get_opt(KQF_CFGO_LOG_TYPE));
	kqf_set_log_level(kqf_get_opt(KQF_CFGO_LOG_LEVEL));
}

void kqf_flush_log(void)
{
	if (LOGINIT_DONE == s_init) {
		EnterCriticalSection(&s_lock);
		if (file_valid()) {
			FlushFileBuffers(s_file);
		}
		LeaveCriticalSection(&s_lock);
	}
}

void kqf_close_log(void)
{
	if (LOGINIT_DONE == s_init) {
		EnterCriticalSection(&s_lock);
		close_file();
		LeaveCriticalSection(&s_lock);
	}
}


KQF_LOGT_ kqf_get_log_type(void)
{
	return (s_type);
}

KQF_LOGT_ kqf_set_log_type(KQF_LOGT_ type)
{
	if ((type != s_type) && (0 <= type) && (type < KQF_LOGT_COUNT)) {
		if (s_init != LOGINIT_DONE)
			init();
		EnterCriticalSection(&s_lock);
		close_file();
		InterlockedCompareExchange(&s_type, type, s_type);
		LeaveCriticalSection(&s_lock);
	}
	return (s_type);
}


KQF_LOGL_ kqf_get_log_level(void)
{
	return (s_level);
}

KQF_LOGL_ kqf_set_log_level(KQF_LOGL_ level)
{
	if ((level != s_level) && (0 <= level) && (level < KQF_LOGL_COUNT)) {
		if (s_init != LOGINIT_DONE)
			init();
		EnterCriticalSection(&s_lock);
		InterlockedCompareExchange(&s_level, level, s_level);
		LeaveCriticalSection(&s_lock);
	}
	return (s_level);
}


void kqf_log(KQF_LOGL_ level, char const *format, ...)
{
	va_list args;
	va_start(args, format);
	if (format && (*format != '\0') && (level <= s_level)) {
		DWORD win_err = GetLastError();
#ifdef KQF_RUNTIME
		int rt_err = MSVCRT_errno;
#endif
		if (s_init != LOGINIT_DONE)
			init();
		EnterCriticalSection(&s_lock);
		c_func[s_type](format, args);
		LeaveCriticalSection(&s_lock);
#ifdef KQF_RUNTIME
		MSVCRT_errno = rt_err;
#endif
		SetLastError(win_err);
	}
	va_end(args);
}

