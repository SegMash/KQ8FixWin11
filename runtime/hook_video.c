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
#include "hook_video.h"
#include "../common/kqf_app.h"
#include "../common/kqf_cfg.h"
#include "../common/kqf_log.h"
#include "hook_cdrom.h"
#include "hook_window.h"


static
int file_exists(char const *name)
{
	WIN32_FIND_DATAA file;
	HANDLE find = FindFirstFileA(name, &file);
	if (find != INVALID_HANDLE_VALUE) {
		FindClose(find);
		return (0 == (FILE_ATTRIBUTE_DIRECTORY & file.dwFileAttributes));
	}
	return (0);
}

static
int redirect_video(char const *old_name, char new_name[MAX_PATH])
{
	if (old_name) {
		int pos = lstrlenA(old_name);
		if (pos < MAX_PATH) {
			pos -= sizeof("_1.dll") - 1;
			if ((pos >= 0) && (0 == lstrcmpiA(&old_name[pos], "_1.dll"))) {
				lstrcpyA(new_name, old_name);
				lstrcpyA(&new_name[pos], "_1.avi");
				return (file_exists(new_name));
			}
		}
	}
	return (0);
}


HWND video_window /* = NULL */;

// Helper structures and callbacks for video window detection
typedef struct {
	HWND hMainWindow;
	HWND videoWindow;
} FindVideoWindowData;


// Callback to find the video player window
BOOL CALLBACK FindVideoWindowProc(HWND hwnd, LPARAM lParam)
{
	if (!IsWindowVisible(hwnd)) return TRUE;
	if (GetParent(hwnd) != NULL) return TRUE; // Skip child windows
	
	FindVideoWindowData* data = (FindVideoWindowData*)lParam;
	if (hwnd == data->hMainWindow) return TRUE; // Skip main game window
	
	// Get window info for debugging
	char windowTitle[256] = {0};
	char className[256] = {0};
	GetWindowTextA(hwnd, windowTitle, sizeof(windowTitle) - 1);
	GetClassNameA(hwnd, className, sizeof(className) - 1);
	
	kqf_log(KQF_LOGL_DEBUG, "Enumerating window: %#08lx, Title: '%.50s', Class: '%.50s'\n", 
		hwnd, windowTitle, className);
	
	// Look for Media Player specifically
	if (strstr(windowTitle, "Media Player") ||
		strstr(windowTitle, "Movies & TV") ||
		strstr(windowTitle, "Films & TV") ||
		strstr(windowTitle, ".avi") ||
		strstr(windowTitle, "VLC") ||
		strstr(className, "ApplicationFrameWindow") ||  // Windows 11 Media Player
		strstr(className, "MediaPlayer") ||
		strstr(className, "VLC")) {
		
		kqf_log(KQF_LOGL_DEBUG, "Found Media Player window: %#08lx, Title: '%.50s', Class: '%.50s'\n", 
			hwnd, windowTitle, className);
		data->videoWindow = hwnd;
		return FALSE; // Stop enumeration
	}
	
	return TRUE; // Continue looking
}


////////////////////////////////////////////////////////////////////////////////
//
//  Redirect the video files (*_1.dll) to *_1.avi if present (this is required
//  for Wine, because the MCI implementation requires the AVI file extension).
//  Due to security issues Microsoft disabled the Indeo Video codec on Windows
//  Vista and newer. You can download re-encoded videos (Cinepak and Microsoft
//  ADPCM are available since Windows 95) from: http://nicode.net/games/kqmoe/
//
//  And while we are at it, we add support for the fake CD-ROM directory here.
//

extern
void *(__cdecl *_imp__fopen)(char const *filename, char const *mode);
void * __cdecl MSVCRT_fopen (char const *filename, char const *mode)
{
	kqf_log(KQF_LOGL_DEBUG, "Playing Video %s %d %d %d\n", filename, strlen(filename), _strnicmp(filename, "w32opn_", 7), _stricmp(filename + 8, ".dll"));
	if ((strlen(filename) == 12) &&
		(_stricmp(filename + 8, ".dll") == 0 || _stricmp(filename + 8, ".avi") == 0)) 
	{
		// Use the actual game window instead of foreground window
		
		HWND hMainWindow = app_window;
		if (!hMainWindow) {
			hMainWindow = GetForegroundWindow();
		}
		
		ShowWindow(hMainWindow, SW_HIDE);
		AllowSetForegroundWindow(ASFW_ANY);
		kqf_log(KQF_LOGL_DEBUG, "Original Window: %#08lx (app_window: %#08lx)\n", hMainWindow, app_window);
		char aviPath[MAX_PATH];
		redirect_video(filename, aviPath);

		
		char command[512];

		// Method 1: Try Windows Media Player first
		strcpy(command, "\"");
		strcat(command, aviPath);
		strcat(command, "\"");
		
		// Store the current foreground window
		HWND originalForeground = GetForegroundWindow();
		
		HINSTANCE result = ShellExecuteA(NULL, "open", command, NULL, NULL, SW_NORMAL);
		kqf_log(KQF_LOGL_DEBUG, " Result code: %d\n", (INT_PTR)result);
		kqf_log(KQF_LOGL_DEBUG, "Playing Video %s instead of %s\n", aviPath, filename);

		// Give media player time to start
		Sleep(3000);
		DWORD startTime = GetTickCount64();
		HWND videoWnd = NULL;
		
        // Active window scanning approach instead of waiting for foreground changes
        while (GetTickCount64() - startTime < 15000) { // 15 second timeout
            kqf_log(KQF_LOGL_DEBUG, "Scanning all windows for Video Player\n");
            
            // Use the existing FindVideoWindowProc callback to scan all windows
            FindVideoWindowData findData;
            findData.hMainWindow = hMainWindow;
            findData.videoWindow = NULL;
            
            // Enumerate all top-level windows to find the media player
            EnumWindows(FindVideoWindowProc, (LPARAM)&findData);
            
            if (findData.videoWindow) {
                videoWnd = findData.videoWindow;
                
                char windowTitle[256] = {0};
                char className[256] = {0};
                GetWindowTextA(videoWnd, windowTitle, sizeof(windowTitle) - 1);
                GetClassNameA(videoWnd, className, sizeof(className) - 1);
                
                kqf_log(KQF_LOGL_DEBUG, "Found video player via scanning: %#08lx, Title: '%.50s', Class: '%.50s'\n", 
                    videoWnd, windowTitle, className);
                
                // Check if it's legacy Media Player vs Windows 11 Media Player
				BOOL isLegacyPlayer = !strstr(className, "ApplicationFrameWindow");
				
				if (isLegacyPlayer) {
					kqf_log(KQF_LOGL_DEBUG, "Detected legacy Media Player - using enhanced fullscreen scaling\n");
					
					// For legacy Media Player, we need to:
					// 1. Set window to fullscreen size
					// 2. Send specific commands to scale video content
					
					// Get screen dimensions
					int screenWidth = GetSystemMetrics(SM_CXSCREEN);
					int screenHeight = GetSystemMetrics(SM_CYSCREEN);
					
					// First bring to front
					SetForegroundWindow(videoWnd);
					BringWindowToTop(videoWnd);
					
					// Set window to exact screen size (borderless fullscreen)
					SetWindowPos(videoWnd, HWND_TOP, 0, 0, screenWidth, screenHeight, 
						SWP_SHOWWINDOW | SWP_FRAMECHANGED);
					
					// Force window style to borderless
					LONG style = GetWindowLongA(videoWnd, GWL_STYLE);
					SetWindowLongA(videoWnd, GWL_STYLE, style & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU));
					
					// Apply the style change
					SetWindowPos(videoWnd, NULL, 0, 0, 0, 0, 
						SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
					
					Sleep(100);
					
					// Try to send fullscreen command via keyboard shortcut (Alt+Enter is common)
					SetFocus(videoWnd);
					keybd_event(VK_MENU, 0, 0, 0);          // Alt down
					keybd_event(VK_RETURN, 0, 0, 0);        // Enter down
					keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0); // Enter up
					keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);   // Alt up
					
					Sleep(100);
					
					// Double-click on video area to trigger fullscreen (common in legacy players)
					RECT windowRect;
					if (GetWindowRect(videoWnd, &windowRect)) {
						int centerX = (windowRect.left + windowRect.right) / 2;
						int centerY = (windowRect.top + windowRect.bottom) / 2;
						
						SetCursorPos(centerX, centerY);
						mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
						mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
						Sleep(50);
						mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
						mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
					}
					
				} else {
					kqf_log(KQF_LOGL_DEBUG, "Detected Windows 11 UWP Media Player - using standard activation\n");
					
					// Standard approach for Windows 11 Media Player
					SetForegroundWindow(videoWnd);
					BringWindowToTop(videoWnd);
					ShowWindow(videoWnd, SW_SHOWMAXIMIZED);
				}
                
                break;
            }
            
            Sleep(500); // Check every 0.5 second
        }
		
		kqf_log(KQF_LOGL_DEBUG, "Final Video Window: %#08lx\n", videoWnd);
		
		if (videoWnd) {
			SetFocus(videoWnd);
			UpdateWindow(videoWnd);
			//SetForegroundWindow(videoWnd);
			// Force maximize the video window with multiple aggressive attempts
			
			
			kqf_log(KQF_LOGL_DEBUG, "Maximizing video window with aggressive methods\n");
			
			// Method 1: Standard approach
			ShowWindow(videoWnd, SW_RESTORE);
			Sleep(50);
			ShowWindow(videoWnd, SW_SHOWMAXIMIZED);
			//SetForegroundWindow(videoWnd);
			BringWindowToTop(videoWnd);
			
			// Method 2: Use SetWindowPos for more control
			
			Sleep(100);
			SetWindowPos(videoWnd, HWND_TOPMOST, 0, 0, 0, 0, 
				SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
			SetWindowPos(videoWnd, HWND_NOTOPMOST, 0, 0, 0, 0, 
				SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
			
			// Method 3: Get screen dimensions and force fullscreen
			

			
			int screenWidth = GetSystemMetrics(SM_CXSCREEN);
			int screenHeight = GetSystemMetrics(SM_CYSCREEN);
			SetWindowPos(videoWnd, HWND_TOP, 0, 0, screenWidth, screenHeight, 
				SWP_SHOWWINDOW);
			
			
			// Method 4: Final maximize attempt
			
			Sleep(100);
			ShowWindow(videoWnd, SW_SHOWMAXIMIZED);
			//SetForegroundWindow(videoWnd);
			SwitchToThisWindow(videoWnd, TRUE);
			
			
			kqf_log(KQF_LOGL_DEBUG, "Completed maximize attempts\n");
			
			
			// Wait for video window to close with timeout
			DWORD videoStartTime = GetTickCount64();
			while (IsWindow(videoWnd) && IsWindowVisible(videoWnd)) {
				// Keep main window hidden while video plays
				//ShowWindow(hMainWindow, SW_HIDE);
				
				// Add timeout to prevent infinite loop (max 10 minutes)
				if (GetTickCount64() - videoStartTime > 600000) {
					kqf_log(KQF_LOGL_DEBUG, "Video timeout reached, breaking\n");
					break;
				}
				
				kqf_log(KQF_LOGL_DEBUG, "Waiting for video to close\n");
				Sleep(500); // Check every 0.5 second
			}
			kqf_log(KQF_LOGL_DEBUG, "Video window closed!\n");
		} else {
			// Fallback: wait a reasonable time if no window was found
			kqf_log(KQF_LOGL_DEBUG, "No video window found, waiting 1 second\n");
			Sleep(1000);
		}
		
		// Simple and clean window restoration
		kqf_log(KQF_LOGL_DEBUG, "Restoring main window: %#08lx\n", hMainWindow);
		
		// Check current window state
		WINDOWPLACEMENT wp;
		wp.length = sizeof(WINDOWPLACEMENT);
		if (GetWindowPlacement(hMainWindow, &wp)) {
			kqf_log(KQF_LOGL_DEBUG, "Window placement showCmd: %d\n", wp.showCmd);
		}
		
		// Simple restoration: Show first, then restore
		BringWindowToTop(hMainWindow);
		SetForegroundWindow(hMainWindow);        // Set as foreground
		ShowWindow(hMainWindow, SW_SHOW);
		Sleep(50);
		ShowWindow(hMainWindow, SW_SHOWMAXIMIZED); // Then maximize
		SetFocus(hMainWindow);
		UpdateWindow(hMainWindow);
		SwitchToThisWindow(hMainWindow, TRUE);
		
		// Make sure it comes to the front above Total Commander
		//SetWindowPos(hMainWindow, HWND_TOP, 0, 0, 0, 0, 
		//	SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
		
		// Bring to foreground
		
		

		
		// Force activation to ensure it's really in front
		
		HWND currentForeground = GetForegroundWindow();
		if (currentForeground != hMainWindow) {
			kqf_log(KQF_LOGL_DEBUG, "Window not in foreground (current: %#08lx), forcing activation\n", currentForeground);
			
			// Simple but effective approach: temporarily make it topmost, then remove topmost
			SetWindowPos(hMainWindow, HWND_TOPMOST, 0, 0, 0, 0, 
				SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
			Sleep(50);
			SetWindowPos(hMainWindow, HWND_NOTOPMOST, 0, 0, 0, 0, 
				SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
			
			// Try to set foreground again
			SetForegroundWindow(hMainWindow);
			
			// Alternative activation method
			SetActiveWindow(hMainWindow);
		}
		
		// Verify restoration worked
		if (IsWindowVisible(hMainWindow)) {
			kqf_log(KQF_LOGL_DEBUG, "Main window is now visible\n");
		} else {
			kqf_log(KQF_LOGL_DEBUG, "WARNING: Main window may still be hidden\n");
		}
		
		// Check final foreground state
		
		HWND finalForeground = GetForegroundWindow();

		kqf_log(KQF_LOGL_DEBUG, "Final foreground window: %#08lx (target: %#08lx)\n", finalForeground, hMainWindow);
		
		kqf_log(KQF_LOGL_DEBUG, "Main window restoration complete\n");
		
		return NULL;
	}

	void *result;
	if (!runtime_active) {
		result = _imp__fopen(filename, mode);
	} else {
		char new_name[MAX_PATH];
		KQF_TRACE("fopen<%#08lx>('%s','%s')\n", ReturnAddress, filename ? filename : "", mode ? mode : "");
		if (filename && mode && (0 == lstrcmpiA(mode, "rb"))) {
			if (kqf_get_opt(KQF_CFGO_CDROM_FAKE) && (0 == lstrcmpiA(filename, FAKE_CDROM "mask.inf"))) {
				if (!file_exists(filename)) {
					kqf_app_filepath("mask.inf", new_name);
					if (!file_exists(new_name))
						kqf_app_filepath("mask.cs", new_name);
					kqf_log(KQF_LOGL_INFO, "fopen: redirect '%s' to '%s'\n", filename, new_name);
					filename = new_name;
				}
			} else if (kqf_get_opt(KQF_CFGO_VIDEO_AVI) && redirect_video(filename, new_name)) {
				kqf_log(KQF_LOGL_INFO, "fopen: redirect '%s' to '%s'\n", filename, new_name);
				filename = new_name;
			}
		}
		result = _imp__fopen(filename, mode);
		KQF_TRACE("fopen<%#08lx>('%s','%s')[%#08lx]{%i}\n", ReturnAddress, filename ? filename : "", mode ? mode : "", result, result ? 0 : MSVCRT_errno);
	}
	return (result);
}

////////////////////////////////////////////////////////////////////////////////
//
//  Redirect the video files (*_1.dll) to *_1.avi if present (this is required
//  for Wine, because the MCI implementation requires the AVI file extension).
//  Due to security issues Microsoft disabled the Indeo Video codec on Windows
//  Vista and newer. You can download re-encoded videos (Cinepak and Microsoft
//  ADPCM are available since Windows 95) from: http://nicode.net/games/kqmoe/
//
//  No border for the MCI window (included in the default style). And add some
//  workarounds for Wine (enforce child window and exclude the default style).
//

HWND VFWAPIV MSVFW32_MCIWndCreateA(HWND hwndParent, HINSTANCE hInstance, DWORD dwStyle, LPCSTR szFile)
{
	HWND result;
	char new_name[MAX_PATH];
	KQF_TRACE("MCIWndCreateA<%#08lx>(%#08lx,%#08lx,%#lx,'%s')\n", ReturnAddress, hwndParent, hInstance, dwStyle, szFile);
	if (kqf_get_opt(KQF_CFGO_VIDEO_AVI) && redirect_video(szFile, new_name)) {
		kqf_log(KQF_LOGL_INFO, "MCIWndCreateA: redirect '%s' to '%s'\n", szFile, new_name);
		szFile = new_name;
	}

	kqf_log(KQF_LOGL_DEBUG, "MCIWndCreateA: '%s'\n", szFile);

	if (MCIWNDF_NOPLAYBAR & dwStyle) {
		// suppress context menu and error dialogs
		dwStyle |= MCIWNDF_NOMENU | MCIWNDF_NOERRORDLG;
	}
	if (kqf_get_opt(KQF_CFGO_VIDEO_NOBORDER) && !HIWORD(dwStyle)) {
		if (hwndParent != NULL) {
			// default child style without WS_BORDER
			dwStyle |= WS_CHILD | WS_VISIBLE;
		} else {
			// default style is WS_OVERLAPPEDWINDOW | WS_VISIBLE
			dwStyle |= WS_POPUP | WS_VISIBLE;
		}
	}
	result = MCIWndCreateA(hwndParent, hInstance, dwStyle, szFile);
	if (result) {
		LONG style = GetWindowLongA(result, GWL_STYLE);
		if (hwndParent != NULL) {
			// Wine is not setting WS_CHILD by default
			if (GetParent(result) != hwndParent) {
				SetWindowLongA(result, GWL_STYLE, (style | WS_CHILD) & ~WS_POPUP);
				SetParent(result, hwndParent);
				kqf_log(KQF_LOGL_NOTICE, "MCIWndCreateA: child window enforced\n");
			}
			// Wine always includes the default styles
			if (kqf_get_opt(KQF_CFGO_VIDEO_NOBORDER) && (WS_BORDER & style)) {
				SetWindowLongA(result, GWL_STYLE, GetWindowLongA(result, GWL_STYLE) & ~WS_CAPTION);
			}
			if (GetWindowLongA(result, GWL_STYLE) != style) {
				SetWindowPos(result, 0, 0, 0, 0, 0, SWP_FRAMECHANGED
					| SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOSENDCHANGING);
				kqf_log(KQF_LOGL_INFO, "MCIWndCreateA: window style overridden (%#lx,%#lx)\n", style, GetWindowLongA(result, GWL_STYLE));
				style = GetWindowLongA(result, GWL_STYLE);
			}
		}
		video_window = result;
	}
	KQF_TRACE("MCIWndCreateA<%#08lx>(%#08lx,%#08lx,%#lx,'%s')[%#08lx]{%#lx}\n", ReturnAddress, hwndParent, hInstance, dwStyle, szFile, result, (result != NULL) ? ERROR_SUCCESS : GetLastError());
	return (result);
}


////////////////////////////////////////////////////////////////////////////////
//
//  The game uses the MoveWindow API only during video playback to move the
//  application window (top/left client area to the top/left of the screen)
//  and the MCI video window. The intention for this window movements seems
//  to be that the video will be played borderless on the screen... But the
//  game uses hard-coded offsets and on current Windows versions it doesn't
//  really work (because the window is enforced back into the screen area).
//

BOOL WINAPI USER32_MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint)
{
	BOOL result = FALSE;
	RECT wr = {0, 0, 640, 480};
	DWORD ws = GetWindowLongA(hWnd, GWL_STYLE);
	KQF_TRACE("MoveWindow<%#08lx>(%#08lx,%i,%i,%i,%i,%i)\n", ReturnAddress, hWnd, X, Y, nWidth, nHeight, bRepaint);
	if (AdjustWindowRectEx(&wr, ws, (WS_CHILD & ws) ? FALSE : (GetMenu(hWnd) != NULL), GetWindowLongA(hWnd, GWL_EXSTYLE))) {
		if ((hWnd == app_window) && kqf_get_opt(KQF_CFGO_VIDEO_NOAPPMOVE)) {
			kqf_log(KQF_LOGL_INFO, "MoveWindow: ignore main window movement\n");
			result = TRUE;
		} else if ((hWnd == video_window) && kqf_get_opt(KQF_CFGO_VIDEO_NOVIDMOVE)) {
			kqf_log(KQF_LOGL_INFO, "MoveWindow: ignore video window movement\n");
			result = TRUE;
		} else {
			X = wr.left;
			Y = wr.top;
			nWidth = wr.right - wr.left;
			nHeight = wr.bottom - wr.top;
			if ((0 == X) && (0 == Y) && (640 == nWidth) && (480 == nHeight)) {
				kqf_log(KQF_LOGL_INFO, "MoveWindow: ignore borderless window movement (%#08lx)\n", hWnd);
				result = TRUE;
			} else {
				kqf_log(KQF_LOGL_INFO, "MoveWindow: adjusted move (%#08lx,%i,%i,%i,%i)\n", hWnd, X, Y, nWidth, nHeight);
			}
		}
	}
	if (!result) {
		result = MoveWindow(hWnd, X, Y, nWidth, nHeight, bRepaint);
	}
	KQF_TRACE("MoveWindow<%#08lx>(%#08lx,%i,%i,%i,%i,%i)[%i]{%#lx}\n", ReturnAddress, hWnd, X, Y, nWidth, nHeight, bRepaint, result, result ? ERROR_SUCCESS : GetLastError());
	return (result);
}
