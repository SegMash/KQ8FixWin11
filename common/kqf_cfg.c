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
#include "kqf_cfg.h"

#include "kqf_app.h"
#include "kqf_log.h"
#include "kqf_win.h"

#pragma warning(push, 1)
# include <limits.h>
#pragma warning(pop)


static
struct opt_inf {
	char const *key;
	int         cnt;
	int         def;
} const opt_inf[KQF_CFGO_COUNT] = {
	{"log.type",        KQF_LOGT_COUNT,           KQF_LOGT_DEFAULT          },  // KQF_CFGO_LOG_TYPE
	{"log.level",       KQF_LOGL_COUNT,           KQF_LOGL_DEFAULT          },  // KQF_CFGO_LOG_LEVEL
	{"mask.dbg",        KQF_OPT_MASK_DBG_COUNT,   KQF_OPT_MASK_DBG_DEFAULT  },  // KQF_CFGO_MASK_DBG
	{"crash.dump",      KQF_OPT_CRASH_DUMP_COUNT, KQF_OPT_CRASH_DUMP_DEFAULT},  // KQF_CFGO_CRASH_DUMP
	{"shim.cbt",        KQF_OPT_BOOL_COUNT,       KQF_OPT_BOOL_TRUE         },  // KQF_CFGO_SHIM_CBT
	{"shim.unmap",      KQF_OPT_BOOL_COUNT,       KQF_OPT_BOOL_TRUE         },  // KQF_CFGO_SHIM_UNMAP
	{"shim.gdfs",       KQF_OPT_BOOL_COUNT,       KQF_OPT_BOOL_TRUE         },  // KQF_CFGO_SHIM_GDFS
	{"shim.gmem",       KQF_OPT_BOOL_COUNT,       KQF_OPT_BOOL_TRUE         },  // KQF_CFGO_SHIM_GMEM
	{"shim.rmdir",      KQF_OPT_BOOL_COUNT,       KQF_OPT_BOOL_TRUE         },  // KQF_CFGO_SHIM_RMDIR
	{"shim.find",       KQF_OPT_BOOL_COUNT,       KQF_OPT_BOOL_TRUE         },  // KQF_CFGO_SHIM_FIND
	{"cdrom.size",      KQF_OPT_BOOL_COUNT,       KQF_OPT_BOOL_TRUE         },  // KQF_CFGO_CDROM_SIZE
	{"talk.complete",   KQF_OPT_BOOL_COUNT,       KQF_OPT_BOOL_TRUE         },  // KQF_CFGO_TALK_COMPLETE
	{"video.avi",       KQF_OPT_BOOL_COUNT,       KQF_OPT_BOOL_TRUE         },  // KQF_CFGO_VIDEO_AVI
	{"video.noborder",  KQF_OPT_BOOL_COUNT,       KQF_OPT_BOOL_TRUE         },  // KQF_CFGO_VIDEO_NOBORDER
	{"video.noappmove", KQF_OPT_BOOL_COUNT,       KQF_OPT_BOOL_TRUE         },  // KQF_CFGO_VIDEO_NOAPPMOVE
	{"video.novidmove", KQF_OPT_BOOL_COUNT,       KQF_OPT_BOOL_TRUE         },  // KQF_CFGO_VIDEO_NOVIDMOVE
	{"glide.nowmsize",  KQF_OPT_BOOL_COUNT,       KQF_OPT_BOOL_TRUE         },  // KQF_CFGO_GLIDE_NOWMSIZE
	{"glide.disable",   KQF_OPT_BOOL_COUNT,       KQF_OPT_BOOL_FALSE        },  // KQF_CFGO_GLIDE_DISABLE
	{"window.title",    KQF_OPT_BOOL_COUNT,       KQF_OPT_BOOL_TRUE         },  // KQF_CFGO_WINDOW_TITLE
	{"window.noborder", KQF_OPT_BOOL_COUNT,       KQF_OPT_BOOL_TRUE         },  // KQF_CFGO_WINDOW_NOBORDER
	{"cdrom.fake",      KQF_OPT_BOOL_COUNT,       KQF_OPT_BOOL_TRUE         },  // KQF_CFGO_CDROM_FAKE
	{"mem.trace",       KQF_OPT_BOOL_COUNT,       KQF_OPT_BOOL_FALSE        },  // KQF_CFGO_MEM_TRACE
	{"text.hebrew.rtl", KQF_OPT_BOOL_COUNT,       KQF_OPT_BOOL_FALSE        }   // KQF_CFGO_TEXT_HEBREW_RTL
};

static
int opt_val[KQF_CFGO_COUNT] = {
	KQF_LOGT_DEFAULT,            // KQF_CFGO_LOG_TYPE
	KQF_LOGL_DEFAULT,            // KQF_CFGO_LOG_LEVEL
	KQF_OPT_MASK_DBG_DEFAULT,    // KQF_CFGO_MASK_DBG
	KQF_OPT_CRASH_DUMP_DEFAULT,  // KQF_CFGO_MASK_DBG
	KQF_OPT_BOOL_TRUE,           // KQF_CFGO_SHIM_CBT
	KQF_OPT_BOOL_TRUE,           // KQF_CFGO_SHIM_UNMAP
	KQF_OPT_BOOL_TRUE,           // KQF_CFGO_SHIM_GDFS
	KQF_OPT_BOOL_TRUE,           // KQF_CFGO_SHIM_GMEM
	KQF_OPT_BOOL_TRUE,           // KQF_CFGO_SHIM_RMDIR
	KQF_OPT_BOOL_TRUE,           // KQF_CFGO_SHIM_FIND
	KQF_OPT_BOOL_TRUE,           // KQF_CFGO_CDROM_SIZE
	KQF_OPT_BOOL_TRUE,           // KQF_CFGO_TALK_COMPLETE
	KQF_OPT_BOOL_TRUE,           // KQF_CFGO_VIDEO_AVI
	KQF_OPT_BOOL_TRUE,           // KQF_CFGO_VIDEO_NOBORDER
	KQF_OPT_BOOL_TRUE,           // KQF_CFGO_VIDEO_NOAPPMOVE
	KQF_OPT_BOOL_TRUE,           // KQF_CFGO_VIDEO_NOVIDMOVE
	KQF_OPT_BOOL_TRUE,           // KQF_CFGO_GLIDE_NOWMSIZE
	KQF_OPT_BOOL_FALSE,          // KQF_CFGO_GLIDE_DISABLE
	KQF_OPT_BOOL_TRUE,           // KQF_CFGO_WINDOW_TITLE
	KQF_OPT_BOOL_TRUE,           // KQF_CFGO_WINDOW_NOBORDER
	KQF_OPT_BOOL_TRUE,           // KQF_CFGO_CDROM_FAKE
	KQF_OPT_BOOL_FALSE,          // KQF_CFGO_MEM_TRACE
	KQF_OPT_BOOL_FALSE           // KQF_CFGO_TEXT_HEBREW_RTL
};


static
char s_path[MAX_PATH] /* = {'\0'} */;

static
char const *ini_path(void)
{
	if ('\0' == s_path[0]) {
		kqf_app_filepath("kq8fix.ini", s_path);
	}
	return (s_path);
}


void kqf_load_cfg(void)
{
	int opt;
	char const *path = ini_path();
	kqf_log(KQF_LOGL_INFO, "Config: loading from '%s'\n", path);
	for (opt = 0; opt < KQF_CFGO_COUNT; ++opt) {
		struct opt_inf const *inf = &opt_inf[opt];
		int val = (INT)GetPrivateProfileIntA("kq8fix", inf->key, -1, path);
		kqf_log(KQF_LOGL_DEBUG, "Config: load '%s' = %d (raw), default = %d\n", inf->key, val, inf->def);
		if ((val < 0) || (val >= inf->cnt)) {
			val = inf->def;
		}
		opt_val[opt] = val;
		kqf_log(KQF_LOGL_DEBUG, "Config: loaded '%s' = %d (final)\n", inf->key, val);
	}
	kqf_log(KQF_LOGL_INFO, "Config: load complete, processed %d options\n", KQF_CFGO_COUNT);
}

void kqf_save_cfg(void)
{
	int opt;
	char const *path = ini_path();
	kqf_log(KQF_LOGL_INFO, "Config: saving to '%s'\n", path);
	for (opt = 0; opt < KQF_CFGO_COUNT; ++opt) {
		CHAR str[10 * sizeof(int) * CHAR_BIT / 33 + 3];
		struct opt_inf const *inf = &opt_inf[opt];
		int val = opt_val[opt];
		int write_val = val;
		if (val == inf->def)
			write_val = -1;
		wsprintfA(str, "%i", write_val);
		kqf_log(KQF_LOGL_DEBUG, "Config: save '%s' = %d (actual) -> %s (written)\n", inf->key, val, str);
		WritePrivateProfileStringA("kq8fix", inf->key, str, path);
	}
	kqf_log(KQF_LOGL_INFO, "Config: save complete, wrote %d options\n", KQF_CFGO_COUNT);
}


int kqf_get_opt(KQF_CFGO_ opt)
{
	return (opt_val[opt]);
}

int kqf_set_opt(KQF_CFGO_ opt, int val)
{
	struct opt_inf const *inf = &opt_inf[opt];
	if ((val < 0) || (inf->cnt <= val))
		val = inf->def;
	opt_val[opt] = val;
	return (val);
}
