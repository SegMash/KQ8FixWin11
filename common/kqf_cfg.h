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
#ifndef KQF_CFG_H_
#define KQF_CFG_H_

#ifdef __cplusplus
extern "C" {
#endif


void kqf_load_cfg(void);  // called by kqf_init
void kqf_save_cfg(void);


typedef enum KQF_OPT_BOOL_ {
	KQF_OPT_BOOL_FALSE,  // 0
	KQF_OPT_BOOL_TRUE,   // 1
	KQF_OPT_BOOL_COUNT
} KQF_OPT_BOOL_;

typedef enum KQF_OPT_MASK_DBG_ {
	KQF_OPT_MASK_DBG_NONE,  // 0 = drop all game debug messages (default)
	KQF_OPT_MASK_DBG_CALL,  // 1 = call the original function
	KQF_OPT_MASK_DBG_LOG,   // 2 = write to log with "[mask.log] " prefix
	KQF_OPT_MASK_DBG_BOTH,  // 3 = CALL + LOG
	KQF_OPT_MASK_DBG_COUNT,
	KQF_OPT_MASK_DBG_DEFAULT = KQF_OPT_MASK_DBG_NONE
} KQF_OPT_MASK_DBG_;

typedef enum KQF_OPT_CRASH_DUMP_ {
	KQF_OPT_CRASH_DUMP_NONE,  // 0 = do not write a minidump file (default)
	KQF_OPT_CRASH_DUMP_NORM,  // 1 = MiniDumpNormal (small)
	KQF_OPT_CRASH_DUMP_DATA,  // 2 = MiniDumpWithDataSegs (large)
	KQF_OPT_CRASH_DUMP_FULL,  // 3 = MiniDumpWithFullMemory (huge)
	KQF_OPT_CRASH_DUMP_COUNT,
	KQF_OPT_CRASH_DUMP_DEFAULT = KQF_OPT_CRASH_DUMP_NONE
} KQF_OPT_CRASH_DUMP_;

typedef enum KQF_CFGO_ {
	KQF_CFGO_LOG_TYPE,         // KQF_LOGT_
	KQF_CFGO_LOG_LEVEL,        // KQF_LOGL_
	KQF_CFGO_MASK_DBG,         // KQF_OPT_MASK_DBG_
	KQF_CFGO_CRASH_DUMP,       // KQF_OPT_CRASH_DUMP_
	KQF_CFGO_SHIM_CBT,         // KQF_OPT_BOOL_
	KQF_CFGO_SHIM_UNMAP,       // KQF_OPT_BOOL_
	KQF_CFGO_SHIM_GDFS,        // KQF_OPT_BOOL_
	KQF_CFGO_SHIM_GMEM,        // KQF_OPT_BOOL_
	KQF_CFGO_SHIM_RMDIR,       // KQF_OPT_BOOL_
	KQF_CFGO_SHIM_FIND,        // KQF_OPT_BOOL_
	KQF_CFGO_CDROM_SIZE,       // KQF_OPT_BOOL_
	KQF_CFGO_TALK_COMPLETE,    // KQF_OPT_BOOL_
	KQF_CFGO_VIDEO_AVI,        // KQF_OPT_BOOL_
	KQF_CFGO_VIDEO_NOBORDER,   // KQF_OPT_BOOL_
	KQF_CFGO_VIDEO_NOAPPMOVE,  // KQF_OPT_BOOL_
	KQF_CFGO_VIDEO_NOVIDMOVE,  // KQF_OPT_BOOL_
	KQF_CFGO_GLIDE_NOWMSIZE,   // KQF_OPT_BOOL_
	KQF_CFGO_GLIDE_DISABLE,    // KQF_OPT_BOOL_
	KQF_CFGO_WINDOW_TITLE,     // KQF_OPT_BOOL_
	KQF_CFGO_WINDOW_NOBORDER,  // KQF_OPT_BOOL_
	KQF_CFGO_CDROM_FAKE,       // KQF_OPT_BOOL_
	KQF_CFGO_MEM_TRACE,        // KQF_OPT_BOOL_
	KQF_CFGO_TEXT_HEBREW_RTL,  // KQF_OPT_BOOL_
	KQF_CFGO_COUNT
} KQF_CFGO_;

int kqf_get_opt(KQF_CFGO_ opt);
int kqf_set_opt(KQF_CFGO_ opt, int val);


#ifdef __cplusplus
}
#endif
#endif
