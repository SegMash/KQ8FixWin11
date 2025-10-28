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
#include "hook_talk.h"
#include "hook_talk.hpp"

#include "../common/kqf_app.h"
#include "../common/kqf_cfg.h"
#include "../common/kqf_log.h"
#include "../common/kqf_win.h"


typedef struct KQTalkMessageCompleteEvent {
	unsigned char _misc[0x001C];  // KQSimEvent
	void         *src;            // KQSound[3D]
	struct msg {
		unsigned short int file;
		unsigned char      noun;
		unsigned char      verb;
		unsigned char      context;
		unsigned char      sequence;
		unsigned char      _misc[6];
	}             msg;
	int           end;
} KQTalkMessageCompleteEvent;

typedef struct KQMonster KQMonster;
typedef struct KQMonsterVtbl {
	void            *_misc[39];
	int (__fastcall *OnTalkMessageComplete)(KQMonster *this, void *edx, KQTalkMessageCompleteEvent *event);
} KQMonsterVtbl;
struct KQMonster {
	KQMonsterVtbl const *__vfptr;
	unsigned char        _misc[0x0FF0];
	void                *OnTalkMessageComplete_src;  // KQSound[3D]
};


static
int use_hash = KQF_OPT_BOOL_TRUE;


static int (__fastcall *mask_KQMonster_OnTalkMessageComplete)(KQMonster *this, void *edx, KQTalkMessageCompleteEvent *event) /* = NULL */;
static int  __fastcall  MASK_KQMonster_OnTalkMessageComplete (KQMonster *this, void *edx, KQTalkMessageCompleteEvent *event)
{
	int result;
	void *old_src = this->OnTalkMessageComplete_src;
	void *hash = (void *)((*(unsigned long *)(&event->msg.noun) ^ (unsigned long)event->src) ^ 0x80000000UL);
	if (use_hash) {
		if (this->OnTalkMessageComplete_src == hash) {
			this->OnTalkMessageComplete_src = event->src;
		} else if (old_src) {
			kqf_log(KQF_LOGL_INFO, "TalkComplete: hash mismatch (%i %i %i %i %i, %i)\n", event->msg.file, event->msg.noun, event->msg.verb, event->msg.context, event->msg.sequence, event->end);
		}
	}
	result = ((int (__fastcall *)(KQMonster *, void *, KQTalkMessageCompleteEvent *))(
		(unsigned long)mask_KQMonster_OnTalkMessageComplete + 0x00000006UL))(
			this, (edx = (void *)TopLevelExceptionHandler, edx), event);
	if (use_hash && this->OnTalkMessageComplete_src) {
		if (this->OnTalkMessageComplete_src == event->src) {
			this->OnTalkMessageComplete_src = hash;
		} else if (!old_src) {
			kqf_log(KQF_LOGL_NOTICE, "TalkComplete: hash disabled (patched binary?)\n");
			use_hash = 0;
		}
	}
	return (result);
}

//
// To avoid patching all the virtual function tables and still support all
// KQMonster classes that use this event handler, the function is directly
// patched with a 'jump' to the MASK_KQMonster_OnTalkMessageComplete(). To
// make the original code more easily callable (__thiscall is wrapped with
// __fastcall) the current/saved SEH frame pointer is now expected in EDX.
//
//  old:
//    00000000  64 A1 00 00 00 00     mov   eax, large fs:0  ; read SEH frame
//    00000006  55                    push  ebp
//    00000007  8B EC                 mov   ebp, esp
//    00000009  6A FF                 push  0FFFFFFFFh
//    0000000B  68 ?? ?? ?? ??        push  offset @@seh
//    00000010  50                    push  eax              ; save SEH frame
//    00000011  64 89 25 00 00 00 00  mov   large fs:0, esp
//  new:
//    00000000  68 xx xx xx xx        push  offset xxxxxxxx  ; push hook addr
//    00000005  C3                    retn                   ; 'jump' to hook
//    00000006  55                    push  ebp
//    00000007  8B EC                 mov   ebp, esp
//    00000009  6A FF                 push  0FFFFFFFFh
//    0000000B  68 ?? ?? ?? ??        push  offset @@seh
//    00000010  52                    push  edx              ; save SEH frame
//    00000011  64 89 25 00 00 00 00  mov   large fs:0, esp
//
//FIXME: [NicoDE] push offset + retn might trigger "security" guards (EMET)
//

static
void hook_KQMonster_OnTalkMessageComplete(void)
{
	unsigned long *ulongs = (unsigned long *)(ULONG_PTR)mask_KQMonster_OnTalkMessageComplete;
	if ((ulongs[0] != 0x0000A164UL) ||
	    (ulongs[1] != 0x8B550000UL) ||
	    (ulongs[2] != 0x68FF6AECUL) ||
	    (ulongs[4] != 0x25896450UL) ||
	    (ulongs[5] != 0x00000000UL)) {
		kqf_log(KQF_LOGL_ERROR, "TalkComplete: unsupported code pattern\n");
	} else {
		MEMORY_BASIC_INFORMATION mem;
		DWORD read_only;
		if (!kqf_query_mem(ulongs, mem))
			mem.Protect = PAGE_NOACCESS;
		read_only = mem.Protect & (PAGE_NOACCESS | PAGE_READONLY | PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_GUARD);
		if (read_only && !VirtualProtect(ulongs, 0x0018UL, PAGE_EXECUTE_READWRITE, &mem.Protect)) {
			kqf_log(KQF_LOGL_ERROR, "TalkComplete: failed to change memory protection (%#lx)\n", GetLastError());
		} else {
			unsigned char *bytes = (unsigned char *)ulongs;
			unsigned long *addr = (unsigned long *)(bytes + 1);
			bytes[0x0000] = 0x68U;
			*addr = (ULONG_PTR)MASK_KQMonster_OnTalkMessageComplete;
			bytes[0x0005] = 0xC3U;
			bytes[0x0010] = 0x52U;
			if (read_only && (mem.Protect != PAGE_NOACCESS))
				VirtualProtect(ulongs, 0x0018UL, mem.Protect, &mem.Protect);
			FlushInstructionCache(GetCurrentProcess(), ulongs, 0x0018UL);
			kqf_log(KQF_LOGL_INFO, "TalkComplete: KQMonster::OnTalkMessageComplete hooked\n");
		}
	}
}

static
void unhook_KQMonster_OnTalkMessageComplete(void)
{
	unsigned char *bytes = (unsigned char *)(ULONG_PTR)mask_KQMonster_OnTalkMessageComplete;
	unsigned long int *addr = (unsigned long *)(bytes + 1);
	if (mask_KQMonster_OnTalkMessageComplete &&
	    (0x68 == bytes[0x0000]) &&
	    (*addr == (ULONG_PTR)MASK_KQMonster_OnTalkMessageComplete) &&
	    (0xC3 == bytes[0x0005]) &&
	    (0x52 == bytes[0x0010])) {
		MEMORY_BASIC_INFORMATION mem;
		DWORD read_only;
		if (!kqf_query_mem(bytes, mem))
			mem.Protect = PAGE_NOACCESS;
		read_only = mem.Protect & (PAGE_NOACCESS | PAGE_READONLY | PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_GUARD);
		if (read_only && !VirtualProtect(bytes, 0x0018UL, PAGE_EXECUTE_READWRITE, &mem.Protect)) {
			kqf_log(KQF_LOGL_ERROR, "TalkComplete: failed to change memory protection (%lx)\n", GetLastError());
		} else {
			bytes[0x0000] = 0x64;
			*addr = 0x000000A1UL;
			bytes[0x0005] = 0x00;
			bytes[0x0010] = 0x50;
			if (read_only && (mem.Protect != PAGE_NOACCESS))
				VirtualProtect(bytes, 0x0018UL, mem.Protect, &mem.Protect);
			FlushInstructionCache(GetCurrentProcess(), bytes, 0x0018UL);
			kqf_log(KQF_LOGL_INFO, "TalkComplete: KQMonster::OnTalkMessageComplete restored\n");
		}
	}
}

void free_talk_complete(void)
{
	unhook_KQMonster_OnTalkMessageComplete();
}


static
int is_conner(void *monster)
{
	// type = MSVCRT.__RTtypeid(monster);
	// - Virtual Function Table pointer (vftable) at offset 0 of the object,
	// - 'RTTI Complete Object Locator' pointer at offset -4 of the vftable,
	// - 'RTTI Type Descriptor' pointer at offset 12 of the complete locator
	MSVCRT_type_info const *type = ((*((void****)monster))[-1])[3];
	return (KQConner_type ? (KQConner_type == type) : (0 == lstrcmpA(type->_m_d_name, ".?AVKQConner@@")));
}

extern
void *(__cdecl *_imp____RTDynamicCast)(void *inptr, long VfDelta, MSVCRT_type_info *SrcType, MSVCRT_type_info *TargetType, int isReference);
void * __cdecl MSVCRT___RTDynamicCast (void *inptr, long VfDelta, MSVCRT_type_info *SrcType, MSVCRT_type_info *TargetType, int isReference)
{
	void *result = _imp____RTDynamicCast(inptr, VfDelta, SrcType, TargetType, isReference);
	if (runtime_active) {
		if (kqf_get_opt(KQF_CFGO_TALK_COMPLETE) && !mask_KQMonster_OnTalkMessageComplete && inptr && SrcType && TargetType) {
			if (result &&
			    ((KQLucreto_type == TargetType) || (
			    ((KQMonster_type == TargetType) && !is_conner(result))))) {
				mask_KQMonster_OnTalkMessageComplete = ((KQMonster *)result)->__vfptr->OnTalkMessageComplete;
			} else
			if ((KQMonster_type == SrcType) && !is_conner(inptr)) {
				mask_KQMonster_OnTalkMessageComplete = ((KQMonster *)inptr)->__vfptr->OnTalkMessageComplete;
			}
			if (mask_KQMonster_OnTalkMessageComplete) {
				kqf_log(KQF_LOGL_INFO, "TalkComplete: KQMonster::OnTalkMessageComplete found (%#08lx)\n", mask_KQMonster_OnTalkMessageComplete);
				hook_KQMonster_OnTalkMessageComplete();
			}
		}
	}
	return (result);
}
