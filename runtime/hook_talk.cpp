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
#include "hook_talk.hpp"

#include "../common/kqf_app.h"
#include "../common/kqf_cfg.h"
#include "../common/kqf_log.h"
#include "../common/kqf_win.h"


#pragma warning(disable: 4483)  // expected C++ keyword
#pragma warning(disable: 4514)  // unreferenced inline function has been removed


MSVCRT_type_info const
*KQConner_type  /* = NULL */,  // ignored (own OnTalkMessageComplete)
*KQMonster_type /* = NULL */,  // TargetType, SrcType
*KQLucreto_type /* = NULL */;  // TargetType

extern "C" extern
char const *(__fastcall *__identifier("_imp_?name@type_info@@QBEPBDXZ"))(MSVCRT_type_info const *This);
char const * __thiscall                    MSVCRT_type_info::name(void) const
{
	char const *result;
	// optimization: only first request (unmangled name empty)
	if (runtime_active) {
		if (this && !this->_m_data) {
			if (kqf_get_opt(KQF_CFGO_TALK_COMPLETE)) {
				if (!KQConner_type && (0 == lstrcmpA(this->_m_d_name, ".?AVKQConner@@"))) {
					KQConner_type = this;
					kqf_log(KQF_LOGL_INFO, "TalkComplete: %s type info found (%#08lx)\n", "KQConner", KQConner_type);
				} else
				if (!KQLucreto_type && (0 == lstrcmpA(this->_m_d_name, ".?AVKQLucreto@@"))) {
					KQLucreto_type = this;
					kqf_log(KQF_LOGL_INFO, "TalkComplete: %s type info found (%#08lx)\n", "KQLucreto", KQLucreto_type);
				} else
				if (!KQMonster_type && (0 == lstrcmpA(this->_m_d_name, ".?AVKQMonster@@"))) {
					KQMonster_type = this;
					kqf_log(KQF_LOGL_INFO, "TalkComplete: %s type info found (%#08lx)\n", "KQMonster", KQMonster_type);
				}
			}
		}
	}
	result = __identifier("_imp_?name@type_info@@QBEPBDXZ")(this);
	return (result);
}

