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
#ifndef HOOK_TALK_H_
#define HOOK_TALK_H_

#ifdef __cplusplus
extern "C" {
#endif


////////////////////////////////////////////////////////////////////////////////
//
//                  Avoid "TalkComplete" bug (cutscene freeze)
//
// The KQTalkMessageCompleteEvent handler of KQMonster (`vftable' + 0x009C) and
// subclasses (KQLucreto, KQDeath, KQDragonBody, KQDragonHead, KQTowerSkeleton,
// KQMandragorTree, KQMandragorRoot, and KQGeometryTalker, but not KQConner) is
// sending a KQTalkMessageCompleteEvent to the active KQStateChangeNotifyGroup.
// To avoid an endless loop the KQMonster scans the group for itself and stores
// the event source (pointer to a KQSound or KQSound3D instance) in a dedicated
// member variable (this + 0x0FF4) if present. The following events are ignored
// as long as the current event source is equal to the saved pointer. This does
// not only skip creating the events, but also the "TalkComplete" script event.
//
// With the Win32 Heap optimizations in Windows 2000 and Windows NT 4.0 Service
// Pack 4 (look-aside lists) and later Windows Vista (low-fragmentation heap is
// enabled by default) memory allocation requests very often return the address
// of a recently freed block with a similar size - this leads to missing events
// whenever the current event source (sound) has the same address as the saved.
//
// Note that it's labeled 'Avoid' and not 'Fix' "TalkComplete" bug, because the
// workaround replaces the saved event source pointer with a hash value that is
// calculated from the event source pointer and the talk event attributes Noun,
// Verb, Context, and Sequence number. Theoretically the hash calculation could
// result in a 0 value and/or the hash of a new event could match the preceding
// event. However, the way how the hash is calculated should abandon both cases
// to theoretical corner cases, that practically should not happen in the game.
//

void free_talk_complete(void);

//
// To support any KQMoE binary (without maintaining an offset list)
// typeid::name() is hooked to capture the RTTI Type Descriptors of
// KQConner, KQMonster, and KQLucreto. Another hook on the internal
// dynamic_cast operator is waiting for an up/down cast from/to the
// KQMonster class (where the object isn't Conner) to determine the
// virtual address of the KQMonster::OnTalkMessageComplete handler.
//

typedef struct MSVCRT_type_info MSVCRT_type_info;
typedef struct MSVCRT_type_infoVtbl {
	void *(__fastcall *__delDtor)(MSVCRT_type_info *This, void *, unsigned flags);
} MSVCRT_type_infoVtbl;
struct MSVCRT_type_info {
	MSVCRT_type_infoVtbl const *__vfptr;
	char                       *_m_data;
	char                        _m_d_name[1], __alignment[sizeof(void *) - 1];
#ifdef __cplusplus
public:
	const char *__thiscall name(void) const;
#endif
};

void *__cdecl MSVCRT___RTDynamicCast(void *inptr, long VfDelta, MSVCRT_type_info *SrcType, MSVCRT_type_info *TargetType, int isReference);


#ifdef __cplusplus
}
#endif
#endif
