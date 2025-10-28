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
#ifndef KQF_LOG_H_
#define KQF_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif


void kqf_init_log(void);  // called by kqf_init
void kqf_flush_log(void);
void kqf_close_log(void);


typedef enum KQF_LOGT_ {
	KQF_LOGT_NULL,  // 0 = discard all log messages
	KQF_LOGT_FILE,  // 1 = write to file "<app>.kq8fix.log"
	KQF_LOGT_ODS,   // 2 = output debug string with "[kq8fix] " prefix
	KQF_LOGT_BOTH,  // 3 = FILE + ODS
	KQF_LOGT_COUNT,
	KQF_LOGT_DEFAULT = KQF_LOGT_FILE
} KQF_LOGT_;

KQF_LOGT_ kqf_get_log_type(void);
KQF_LOGT_ kqf_set_log_type(KQF_LOGT_ type);


typedef enum KQF_LOGL_ {
	KQF_LOGL_EMERGENCY,  // 0
	KQF_LOGL_ALERT,      // 1
	KQF_LOGL_CRITICAL,   // 2
	KQF_LOGL_ERROR,      // 3
	KQF_LOGL_WARNING,    // 4
	KQF_LOGL_NOTICE,     // 5
	KQF_LOGL_INFO,       // 6
	KQF_LOGL_DEBUG,      // 7
	KQF_LOGL_TRACE,      // 8 (excluded in release builds)
	KQF_LOGL_COUNT,
	KQF_LOGL_DEFAULT = KQF_LOGL_WARNING,
	KQF_LOGL_FORCE = -1
} KQF_LOGL_;

KQF_LOGL_ kqf_get_log_level(void);
KQF_LOGL_ kqf_set_log_level(KQF_LOGL_ level);

void kqf_log(KQF_LOGL_ level, char const *format, ...);


#ifdef KQF_DEBUG
# define KQF_TRACE(format, ...)        kqf_log(KQF_LOGL_TRACE, format, __VA_ARGS__)
# define KQF_TRACE_FIND_N(format, ...) ((void)0)
# define KQF_TRACE_WINDOW(format, ...) ((void)0)
#else
# define KQF_TRACE(format, ...)        ((void)0)
# define KQF_TRACE_FIND_N(format, ...) ((void)0)
# define KQF_TRACE_WINDOW(format, ...) ((void)0)
#endif


#ifdef __cplusplus
}
#endif
#endif
