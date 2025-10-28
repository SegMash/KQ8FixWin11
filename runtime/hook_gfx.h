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
#ifndef HOOK_GFX_H
#define HOOK_GFX_H

#include "../common/kqf_win.h"

#ifdef __cplusplus
extern "C" {
#endif


////////////////////////////////////////////////////////////////////////////////
//
//             Disable 3Dfx if requested (Direct3D mode available)
//

HMODULE WINAPI KERNEL32_LoadLibraryA(LPCSTR lpLibFileName);


////////////////////////////////////////////////////////////////////////////////
//
//                 Handle NULL pointer access in GFXClearScreen
//
// Happened on Wine 1.8.1 after loading a saved game in Software (Direct Draw).
// Looks like the game silently expects to sucessfully lock the surface memory.
//

void hook_GFXClearScreen(void);
void unhook_GFXClearScreen(void);


////////////////////////////////////////////////////////////////////////////////
//
//                Fix possible crash in Direct3D initialization
//
// During the enumeration of the D3D display modes the wrong index (valid device
// index instead of an absolute device index) is passed to an internal function.
//

void patch_D3DTotalVideoMemory(void);


////////////////////////////////////////////////////////////////////////////////
//
//                         Fix brightness slider option
//
// Due to a rounding error the brightness is decreased when options are saved.
//

void patch_BrightnessSlider(void);


#ifdef __cplusplus
}
#endif
#endif
