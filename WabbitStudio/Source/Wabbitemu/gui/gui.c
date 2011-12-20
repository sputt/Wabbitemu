#include "stdafx.h"

#include <io.h>
#include <fcntl.h>


#include "gui.h"
#include "resource.h"
#include "uxtheme.h"

#include "core.h"
#include "calc.h"
#include "label.h"

#include "gifhandle.h"
#include "gif.h"

#include "var.h"
#include "link.h"
#include "keys.h"
#include "fileutilities.h"
#include "exportvar.h"

#include "dbmem.h"
#include "dbreg.h"
#include "dbtoolbar.h"
#include "dbdisasm.h"
#include "dbwatch.h"

#include "guibuttons.h"
#include "guicontext.h"
#include "guicutout.h"
#include "guidebug.h"
#include "guidetached.h"
#include "guifaceplate.h"
#include "guiglow.h"
#include "guikeylist.h"
#include "guilcd.h"
#include "guiopenfile.h"
#include "guioptions.h"
#include "guisavestate.h"
#include "guispeed.h"
#include "guivartree.h"
#include "guiwizard.h"

#include "DropTarget.h"
#include "expandpane.h"
#include "registry.h"
#include "sendfileswindows.h"
#include "state.h"
#include "avi_utils.h"
#include "CGdiPlusBitmap.h"


#ifdef _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#define MENU_FILE 0
#define MENU_EDIT 1
#define MENU_CALC 2
#define MENU_HELP 3

TCHAR ExeDir[512];

INT_PTR CALLBACK DlgVarlist(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
HINSTANCE g_hInst;
HACCEL hacceldebug;
HACCEL haccelmain;
POINT drop_pt;
BOOL gif_anim_advance;
BOOL silent_mode = FALSE;
BOOL is_exiting = FALSE;
HIMAGELIST hImageList = NULL;

LRESULT CALLBACK WndProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ToolProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);

INT_PTR CALLBACK AboutDialogProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK ExportOSDialogProc(HWND, UINT, WPARAM, LPARAM);


void gui_draw(calc_t *lpCalc) {
	if (lpCalc->hwndLCD != NULL) {
		InvalidateRect(lpCalc->hwndLCD, NULL, FALSE);
	}

	if (lpCalc->hwndDetachedLCD != NULL) {
		InvalidateRect(lpCalc->hwndDetachedLCD, NULL, FALSE);
	}

	if (lpCalc->gif_disp_state != GDS_IDLE) {
		static int skip = 0;
		if (skip == 0) {
			gif_anim_advance = TRUE;
			if (lpCalc->hwndFrame != NULL) {
				InvalidateRect(lpCalc->hwndFrame, NULL, FALSE);
			}
		}
		
		skip = (skip + 1) % 4;
	}
}

VOID CALLBACK TimerProc(HWND hwnd, UINT Message, UINT_PTR idEvent, DWORD dwTimer) {
	static long difference;
	static DWORD prevTimer;

	// How different the timer is from where it should be
	// guard from erroneous timer calls with an upper bound
	// that's the limit of time it will take before the
	// calc gives up and claims it lost time
	difference += ((dwTimer - prevTimer) & 0x003F) - TPF;
	prevTimer = dwTimer;

	// Are we greater than Ticks Per Frame that would call for
	// a frame skip?
	if (difference > -TPF) {
		calc_run_all();
		while (difference >= TPF) {
			calc_run_all();
			difference -= TPF;
		}

		int i;
		for (i = 0; i < MAX_CALCS; i++) {
			if (calcs[i].active) {
				gui_draw(&calcs[i]);
			}
		}
	// Frame skip if we're too far ahead.
	} else difference += TPF;
}

extern WINDOWPLACEMENT db_placement;

HWND gui_debug(LPCALC lpCalc) {
	TCHAR buf[256];
	if (link_connected_hub(lpCalc->slot))
		StringCbPrintf(buf, sizeof(buf), _T("Debugger (%d)"), lpCalc->slot + 1);
	else
		StringCbCopy(buf, sizeof(buf), _T("Debugger"));
	if (lpCalc->audio != NULL)
		pausesound(lpCalc->audio);
	HWND hdebug;
	BOOL set_place = TRUE;
	int flags = 0;
	RECT pos = {CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT+800, CW_USEDEFAULT+600};
	if (!db_placement.length) {
		db_placement.flags = SW_SHOWNORMAL;
		db_placement.length = sizeof(WINDOWPLACEMENT);
		CopyRect(&db_placement.rcNormalPosition, &pos);
		set_place = FALSE;
		flags = WS_VISIBLE;
	}

	pos.right -= pos.left;
	pos.bottom -= pos.top;
	

	lpCalc->running = FALSE;
	calc_pause_linked();
	if (hdebug = FindWindow(g_szDebugName, buf)) {
		if (lpCalc != lpDebuggerCalc) {
			DestroyWindow(hdebug);
		} else {
			SwitchToThisWindow(hdebug, TRUE);
			SendMessage(hdebug, WM_USER, DB_RESUME, 0);
			return hdebug;
		}
	}
	
	hdebug = CreateWindowEx(
		WS_EX_APPWINDOW,
		g_szDebugName,
		buf,
		flags | WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		pos.left, pos.top, pos.right, pos.bottom,
		0, 0, g_hInst, (LPVOID) lpCalc);
	if (set_place)
		SetWindowPlacement(hdebug, &db_placement);

	lpCalc->hwndDebug = hdebug;
	SendMessage(hdebug, WM_SIZE, 0, 0);
	return hdebug;
}

int gui_frame(LPCALC lpCalc) {
	RECT r;

	if (!lpCalc->scale)
		lpCalc->scale = 2;
	if (lpCalc->SkinEnabled) {
		SetRect(&r, 0, 0, lpCalc->rectSkin.right, lpCalc->rectSkin.bottom);
	} else {
		SetRect(&r, 0, 0, 128 * lpCalc->scale, 64 * lpCalc->scale);
	}
	AdjustWindowRect(&r, WS_CAPTION | WS_TILEDWINDOW, FALSE);
	r.bottom += GetSystemMetrics(SM_CYMENU);

	lpCalc->hwndFrame = CreateWindowEx(
		0, //WS_EX_APPWINDOW,
		g_szAppName,
		_T("Z80"),
		(WS_TILEDWINDOW |  (silent_mode ? 0 : WS_VISIBLE) | WS_CLIPCHILDREN) & ~(WS_MAXIMIZEBOX /* | WS_SIZEBOX */),
		startX, startY, r.right - r.left, r.bottom - r.top,
		NULL, 0, g_hInst, (LPVOID) lpCalc);

	SetWindowText(lpCalc->hwndFrame, _T("Wabbitemu"));
	HDC hdc = GetDC(lpCalc->hwndFrame);
	lpCalc->hdcSkin = CreateCompatibleDC(hdc);
	lpCalc->breakpoint_callback = gui_debug;

	//this is now (intuitively) created in guicutout.c (Enable/Disable cutout function)
	/*lpCalc->hwndLCD = CreateWindowEx(
		0,
		g_szLCDName,
		"LCD",
		WS_VISIBLE |  WS_CHILD,
		0, 0, lpCalc->cpu.pio.lcd->width*lpCalc->Scale, 64*lpCalc->Scale,
		lpCalc->hwndFrame, (HMENU) 99, g_hInst,  NULL);*/

	if (lpCalc->hwndFrame == NULL /*|| lpCalc->hwndLCD == NULL*/) return -1;

	GetClientRect(lpCalc->hwndFrame, &r);
	lpCalc->running = TRUE;
	lpCalc->speed = 100;
	HMENU hmenu = GetMenu(lpCalc->hwndFrame);
	CheckMenuRadioItem(GetSubMenu(hmenu, 2), IDM_SPEED_QUARTER, IDM_SPEED_MAX, IDM_SPEED_NORMAL, MF_BYCOMMAND);
	gui_frame_update(lpCalc);
	ReleaseDC(lpCalc->hwndFrame, hdc);
	return 0;
}

BOOL FindLCDRect(Bitmap *m_pBitmapKeymap, int skinWidth, int skinHeight, RECT *rectLCD) {
	BOOL foundScreen = FALSE;
	u_int foundX, foundY;
	Color pixel;
	//find the top left corner
	for (u_int y = 0; y < skinHeight && foundScreen == false; y++) {
		for (u_int x = 0; x < skinWidth && foundScreen == false; x++) {
			m_pBitmapKeymap->GetPixel(x, y, &pixel);
			if (pixel.GetValue() == 0xFFFF0000)	{
				//81 92
				foundX = x;
				foundY = y;
				foundScreen = true;
			}
		}
	}
	if (!foundScreen) {
		return foundScreen;
	}
	rectLCD->left = foundX;
	rectLCD->top = foundY;
	//find right edge
	do {
		foundX++;
		m_pBitmapKeymap->GetPixel(foundX, foundY, &pixel);
	} while (pixel.GetValue() == 0xFFFF0000);
	rectLCD->right = foundX--;
	//find left edge
	do { 
		foundY++;
		m_pBitmapKeymap->GetPixel(foundX, foundY, &pixel);
	} while (pixel.GetValue() == 0xFFFF0000);
	rectLCD->bottom = foundY;
	return foundScreen;
}

void UpdateWabbitemuMainWindow(LPCALC lpCalc) {
	HMENU hMenu = GetMenu(lpCalc->hwndFrame);
	if (lpCalc->SkinEnabled) {
		if (hMenu) {
			CheckMenuItem(hMenu, IDM_VIEW_SKIN, MF_BYCOMMAND | MF_CHECKED);
		}
		DestroyWindow(lpCalc->hwndStatusBar);
		CloseWindow(lpCalc->hwndStatusBar);
		lpCalc->hwndStatusBar = NULL;
		RECT rc;
		CopyRect(&rc, &lpCalc->rectSkin);
		AdjustWindowRect(&rc, WS_CAPTION | WS_TILEDWINDOW , FALSE);
		rc.bottom += GetSystemMetrics(SM_CYMENU);
		SetWindowPos(lpCalc->hwndFrame, NULL, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER | SWP_NOMOVE);
	} else {
		RECT rc;
		if (hMenu) {
			CheckMenuItem(hMenu, IDM_VIEW_SKIN, MF_BYCOMMAND | MF_UNCHECKED);
		}
		// Create status bar
		if (lpCalc->hwndStatusBar != NULL) {
			DestroyWindow(lpCalc->hwndStatusBar);
			CloseWindow(lpCalc->hwndStatusBar);
		}
		SetRect(&rc, 0, 0, 128 * lpCalc->scale, 64 * lpCalc->scale);
		int iStatusWidths[] = { 100, -1 };
		lpCalc->hwndStatusBar = CreateWindowEx(0, STATUSCLASSNAME, NULL, WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, lpCalc->hwndFrame, (HMENU) 99, g_hInst, NULL);
		SendMessage(lpCalc->hwndStatusBar, SB_SETPARTS, 2, (LPARAM) &iStatusWidths);
		SendMessage(lpCalc->hwndStatusBar, SB_SETTEXT, 1, (LPARAM) CalcModelTxt[lpCalc->model]);
		RECT src;
		GetWindowRect(lpCalc->hwndStatusBar, &src);
		AdjustWindowRect(&rc, (WS_TILEDWINDOW | WS_CLIPCHILDREN) & ~WS_MAXIMIZEBOX, hMenu != NULL);
		rc.bottom += src.bottom - src.top;
		SetWindowPos(lpCalc->hwndFrame, NULL, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);
		GetClientRect(lpCalc->hwndFrame, &rc);
		SendMessage(lpCalc->hwndStatusBar, WM_SIZE, 0, 0);
		SendMessage(lpCalc->hwndStatusBar, SB_SETTEXT, 1, (LPARAM) CalcModelTxt[lpCalc->model]);
	}

	if (lpCalc->bAlwaysOnTop) {
		if (!(GetWindowLong(lpCalc->hwndFrame, GWL_EXSTYLE) & WS_EX_TOPMOST)) {
			SetWindowPos(lpCalc->hwndFrame, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}
	} else {
		if (GetWindowLong(lpCalc->hwndFrame, GWL_EXSTYLE) & WS_EX_TOPMOST) {
			SetWindowPos(lpCalc->hwndFrame, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}
	}
}

enum DRAWSKINERROR {
	ERROR_FACEPLATE = 1,
	ERROR_CUTOUT
};

DRAWSKINERROR DrawSkin(HDC hdc, LPCALC lpCalc, Bitmap *m_pBitmapSkin, Bitmap *m_pBitmapKeymap) {
	HBITMAP hbmSkinOld, hbmKeymapOld;
	//translate to regular gdi compatibility to simplify coding :/
	m_pBitmapKeymap->GetHBITMAP(Color::White, &hbmKeymapOld);
	SelectObject(lpCalc->hdcKeymap, hbmKeymapOld);
	//get the HBITMAP for the skin DONT change the first value, it is necessary for transparency to work
	m_pBitmapSkin->GetHBITMAP(Color::AlphaMask, &hbmSkinOld);
	//84+SE has custom faceplates :D, draw it to the background
	//thanks MSDN your documentation rules :))))
	HDC hdcOverlay = CreateCompatibleDC(lpCalc->hdcSkin);
	HBITMAP blankBitmap = CreateCompatibleBitmap(hdc, m_pBitmapSkin->GetWidth(), m_pBitmapSkin->GetHeight());
	SelectObject(lpCalc->hdcSkin, blankBitmap);
	if (!lpCalc->bCutout || !lpCalc->SkinEnabled)
		FillRect(lpCalc->hdcSkin, &lpCalc->rectSkin, GetStockBrush(GRAY_BRUSH));
	if (lpCalc->model == TI_84PSE) {
		if (DrawFaceplateRegion(lpCalc->hdcSkin, lpCalc->FaceplateColor)) {
			return ERROR_FACEPLATE;
		}
	}

	//this needs to be done so we can alpha blend the screen
	HBITMAP oldSkin = (HBITMAP) SelectObject(hdcOverlay, hbmSkinOld);
	BLENDFUNCTION bf;
	bf.BlendOp = AC_SRC_OVER;
	bf.BlendFlags = 0;
	bf.SourceConstantAlpha = 255;
	bf.AlphaFormat = AC_SRC_ALPHA;
	AlphaBlend(lpCalc->hdcSkin, 0, 0, lpCalc->rectSkin.right, lpCalc->rectSkin.bottom, hdcOverlay,
		lpCalc->rectSkin.left, lpCalc->rectSkin.top, lpCalc->rectSkin.right, lpCalc->rectSkin.bottom, bf);
	BitBlt(lpCalc->hdcButtons, 0, 0, lpCalc->rectSkin.right, lpCalc->rectSkin.bottom, lpCalc->hdcSkin, 0, 0, SRCCOPY);
	FinalizeButtons(lpCalc);
	if (lpCalc->bCutout && lpCalc->SkinEnabled)	{
		if (EnableCutout(lpCalc) != 0) {
			return ERROR_CUTOUT;
		}
		//TODO: figure out why this needs to be called again
		EnableCutout(lpCalc);
	} else {
		DisableCutout(lpCalc->hwndFrame);
	}

	DeleteObject(hbmKeymapOld);
	DeleteObject(hbmSkinOld);
	DeleteObject(blankBitmap);
	DeleteDC(hdcOverlay);

	return (DRAWSKINERROR) ERROR_SUCCESS;
}

int gui_frame_update(LPCALC lpCalc) {
	int skinWidth = 0, skinHeight = 0, keymapWidth = -1, keymapHeight = -1;
	HDC hdc = GetDC(lpCalc->hwndFrame);
	if (lpCalc->hdcKeymap) {
		DeleteDC(lpCalc->hdcKeymap);
	}
	if (lpCalc->hdcSkin) {
		DeleteDC(lpCalc->hdcSkin);
	}
	if (lpCalc->hdcButtons) {
		DeleteDC(lpCalc->hdcButtons);
	}
	lpCalc->hdcKeymap = CreateCompatibleDC(hdc);
	lpCalc->hdcSkin = CreateCompatibleDC(hdc);
	lpCalc->hdcButtons = CreateCompatibleDC(hdc);
	//load skin and keymap
	CGdiPlusBitmapResource hbmSkin, hbmKeymap;
	Bitmap *m_pBitmapSkin = NULL, *m_pBitmapKeymap = NULL;
	if (lpCalc->bCustomSkin) {
#ifdef _UNICODE
		m_pBitmapSkin = new Bitmap(lpCalc->skin_path);
		m_pBitmapKeymap = new Bitmap(lpCalc->keymap_path);
#else
		wchar_t widePath[MAX_PATH];
		size_t converted;
		mbstowcs_s(&converted, widePath, lpCalc->skin_path, (size_t) ARRAYSIZE(widePath));
		m_pBitmapSkin = new Bitmap(widePath);
		mbstowcs_s(&converted, widePath, lpCalc->keymap_path, (size_t) ARRAYSIZE(widePath));
		m_pBitmapKeymap = new Bitmap(widePath);
#endif
	}
	if (!m_pBitmapSkin || m_pBitmapSkin->GetWidth() == 0 || m_pBitmapKeymap->GetWidth() == 0) {
		if (lpCalc->bCustomSkin) {
			MessageBox(lpCalc->hwndFrame, _T("Custom skin failed to load."), _T("Error"),  MB_OK);
			delete m_pBitmapKeymap;
			delete m_pBitmapSkin;
			m_pBitmapKeymap = NULL;
			m_pBitmapSkin = NULL;
			//your skin failed to load, lets disable it and load the normal skin
			lpCalc->bCustomSkin = FALSE;
		}
		hbmSkin.Load(CalcModelTxt[lpCalc->model], _T("PNG"), g_hInst);
		switch(lpCalc->model) {
			case TI_81:
			case TI_82:
				hbmKeymap.Load(_T("TI-82Keymap"), _T("PNG"), g_hInst);
				break;
			case TI_83:
				hbmKeymap.Load(_T("TI-83Keymap"), _T("PNG"), g_hInst);
				break;
			case TI_84P:
			case TI_84PSE:
				hbmKeymap.Load(_T("TI-84+SEKeymap"), _T("PNG"), g_hInst);
				break;
			case TI_85:
				hbmKeymap.Load(_T("TI-85Keymap"), _T("PNG"), g_hInst);
				break;
			case TI_86:
				hbmKeymap.Load(_T("TI-86Keymap"), _T("PNG"), g_hInst);
				break;
			case TI_73:
			case TI_83P:
			case TI_83PSE:
			default:
				hbmKeymap.Load(_T("TI-83+Keymap"), _T("PNG"), g_hInst);
				break;
		}
		m_pBitmapSkin = hbmSkin.m_pBitmap;
		m_pBitmapKeymap = hbmKeymap.m_pBitmap;
	}

	if (m_pBitmapSkin) {
		skinWidth = m_pBitmapSkin->GetWidth();
		skinHeight = m_pBitmapSkin->GetHeight();
	}
	if (m_pBitmapKeymap) {
		keymapWidth = m_pBitmapKeymap->GetWidth();
		keymapHeight = m_pBitmapKeymap->GetHeight();
	}
	int x, y, foundX = 0, foundY = 0;
	bool foundScreen = FALSE;
	if ((skinWidth != keymapWidth) || (skinHeight != keymapHeight) || skinHeight <= 0 || skinWidth <= 0) {
		lpCalc->SkinEnabled = false;
		MessageBox(lpCalc->hwndFrame, _T("Skin and Keymap are not the same size"), _T("Error"),  MB_OK);
	} else {
		lpCalc->rectSkin.right = skinWidth;
		lpCalc->rectSkin.bottom = skinHeight;
		foundScreen = FindLCDRect(m_pBitmapKeymap, skinWidth, skinHeight, &lpCalc->rectLCD);
	}
	if (!foundScreen) {
		MessageBox(lpCalc->hwndFrame, _T("Unable to find the screen box"), _T("Error"), MB_OK);
		lpCalc->SkinEnabled = false;
	}
	if (!lpCalc->hwndFrame) {
		return 0;
	}

	//set the size of the HDC
	HBITMAP hbmTemp = CreateCompatibleBitmap(hdc, lpCalc->rectSkin.right, lpCalc->rectSkin.bottom);
	SelectObject(lpCalc->hdcButtons, hbmTemp);
	DeleteObject(hbmTemp);

	UpdateWabbitemuMainWindow(lpCalc);
	
	switch (DrawSkin(hdc, lpCalc, m_pBitmapSkin, m_pBitmapKeymap)) {
		case ERROR_FACEPLATE:
			MessageBox(lpCalc->hwndFrame, _T("Unable to draw faceplate"), _T("Error"), MB_OK);
			break;
		case ERROR_CUTOUT:
			MessageBox(lpCalc->hwndFrame, _T("Couldn't cutout window"), _T("Error"),  MB_OK);
			break;
	}
	if (lpCalc->bCustomSkin) {
		if (m_pBitmapKeymap) {
			delete m_pBitmapKeymap;
		}
		if (m_pBitmapSkin) {
			delete m_pBitmapSkin;
		}
	}
	ReleaseDC(lpCalc->hwndFrame, hdc);

	if (lpCalc->hwndStatusBar != NULL) {
		SendMessage(lpCalc->hwndStatusBar, SB_SETTEXT, 1, (LPARAM) CalcModelTxt[lpCalc->model]);
	}
	SendMessage(lpCalc->hwndFrame, WM_SIZE, 0, 0);

	return 0;
}

/*
 * Searches for a window with Wabbit's registered lcd class
 */
HWND find_existing_lcd(HWND hwndParent) 
{
	HWND FindChildhwnd = FindWindowEx(hwndParent, NULL, g_szLCDName, NULL);
	if (FindChildhwnd == NULL)
		FindChildhwnd = FindWindowEx(NULL, NULL, g_szLCDName, NULL);
	return FindChildhwnd;
}

/*
 * Checks based on the existence of the main window and the LCD window whether we need
 * to spawn a new process
 * returns false if there is no existing process
 * returns true if there is an existing process found
 */
bool check_no_new_process(HWND Findhwnd) {
	if (Findhwnd == NULL) {
		return false;
	} else {
		return find_existing_lcd(Findhwnd) != NULL;
	}
}

extern HWND hwndProp;
extern RECT PropRect;
extern int PropPageLast;

void RegisterWindowClasses(void) {
	WNDCLASSEX wc;

	wc.cbSize = sizeof(wc);
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = g_hInst;
	wc.hIcon = LoadIcon(g_hInst, _T("W"));
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = MAKEINTRESOURCE(IDR_MAIN_MENU);
	wc.lpszClassName = g_szAppName;
	wc.hIconSm = LoadIcon(g_hInst, _T("W"));

	RegisterClassEx(&wc);

	// LCD
	wc.lpszClassName = g_szLCDName;
	wc.lpfnWndProc = LCDProc;
	wc.lpszMenuName = NULL;
	RegisterClassEx(&wc);

	// Toolbar
	wc.lpszClassName = g_szToolbar;
	wc.lpfnWndProc = ToolBarProc;
	wc.lpszMenuName = NULL;
	wc.style = 0;
	RegisterClassEx(&wc);

	// Debugger
	wc.lpfnWndProc = DebugProc;
	wc.style = CS_DBLCLKS;
	wc.lpszClassName = g_szDebugName;
	wc.lpszMenuName = MAKEINTRESOURCE(IDR_DEBUG_MENU);
	wc.hbrBackground = (HBRUSH) (COLOR_BTNFACE+1);
	RegisterClassEx(&wc);

	// Disassembly
	wc.lpszMenuName = NULL;
	wc.style = 0;
	wc.lpfnWndProc = DisasmProc;
	wc.lpszClassName = g_szDisasmName;
	wc.hbrBackground = (HBRUSH) NULL;
	RegisterClassEx(&wc);

	// Registers
	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = RegProc;
	wc.lpszClassName = g_szRegName;
	wc.hbrBackground = NULL;
	RegisterClassEx(&wc);

	// Expanding Panes
	wc.style = 0;
	wc.lpfnWndProc = ExpandPaneProc;
	wc.lpszClassName = g_szExpandPane;
	RegisterClassEx(&wc);

	// Memory Viewer
	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = MemProc;
	wc.lpszClassName = g_szMemName;
	wc.hbrBackground = NULL;
	RegisterClassEx(&wc);

	// Watchpoints
	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = WatchProc;
	wc.lpszClassName = g_szWatchName;
	wc.hbrBackground = NULL;
	RegisterClassEx(&wc);

	// Detached LCD
	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = DetachedProc;
	wc.lpszClassName = g_szDetachedName;
	wc.hbrBackground = NULL;
	RegisterClassEx(&wc);

	// Small cutout buttons
	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = SmallButtonProc;
	wc.lpszClassName = g_szSmallButtonsName;
	wc.hbrBackground = NULL;
	RegisterClassEx(&wc);
}

#define MAX_FILES 255
struct ParsedCmdArgs
{
	LPTSTR rom_files[MAX_FILES];
	LPTSTR utility_files[MAX_FILES];
	LPTSTR archive_files[MAX_FILES];
	LPTSTR ram_files[MAX_FILES];
	int num_rom_files;
	int num_utility_files;
	int num_archive_files;
	int num_ram_files;
	BOOL silent_mode;
	BOOL force_new_instance;
	BOOL force_focus;
};

ParsedCmdArgs* ParseCommandLineArgs()
{
	ParsedCmdArgs *parsedArgs = (ParsedCmdArgs *) malloc(sizeof(ParsedCmdArgs));
	ZeroMemory(parsedArgs, sizeof(ParsedCmdArgs));
	TCHAR tmpstring[512];
	SEND_FLAG ram = SEND_CUR;
	int argc;
	LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);


	if (argv && argc > 1) {
#ifdef _UNICODE
		StringCbCopy(tmpstring, sizeof(tmpstring), argv[1]);
#else
		size_t numConv;
		wcstombs_s(&numConv, tmpstring, argv[1], 512);
#endif
		TCHAR* FileNames = NULL;
		for(int i = 1; i < argc; i++) {
			memset(tmpstring, 0, 512);
#ifdef _UNICODE
			_tcscpy(tmpstring, argv[i]);
#else
			size_t numConv;
			wcstombs_s(&numConv, tmpstring, argv[i], 512);
#endif
			char secondChar = toupper(tmpstring[1]);
			if (*tmpstring != '-' && *tmpstring != '/') {
				TCHAR *temp = (TCHAR *) malloc(strlen(tmpstring) + 1);
				StringCbCopy(temp, strlen(tmpstring) + 1, tmpstring);
				temp[strlen(tmpstring) + 1] = '\0';
				TCHAR extension[5] = _T("");
				const TCHAR *pext = _tcsrchr(tmpstring, _T('.'));
				if (pext != NULL) {
					StringCbCopy(extension, sizeof(extension), pext);
				}
				if (!_tcsicmp(extension, _T(".rom")) || !_tcsicmp(extension, _T(".sav")) || !_tcsicmp(extension, _T(".clc"))) {
					parsedArgs->rom_files[parsedArgs->num_rom_files++] = temp;
				}
				else if (!_tcsicmp(extension, _T(".brk")) || !_tcsicmp(extension, _T(".lab")) 
					|| !_tcsicmp(extension, _T(".zip")) || !_tcsicmp(extension, _T(".tig"))) {
						parsedArgs->utility_files[parsedArgs->num_utility_files++] = temp;
				}
				else if (ram) {
					parsedArgs->ram_files[parsedArgs->num_ram_files++] = temp;
				} else {
					parsedArgs->archive_files[parsedArgs->num_archive_files++] = temp;
				}
			} else if (secondChar == 'R') {
				ram = SEND_RAM;
			} else if (secondChar == 'A') {
				ram = SEND_ARC;
			} else if (secondChar == 'S') {
				parsedArgs->silent_mode = TRUE;
			} else if (secondChar == 'F') {
				parsedArgs->force_focus = TRUE;
			} else if (secondChar == 'N') {
				parsedArgs->force_new_instance = TRUE;
			}
		}
	}
	return parsedArgs;
}

void LoadAlreadyExistingWabbit(LPARAM lParam, LPTSTR filePath, SEND_FLAG sendLoc)
{
	HWND hwnd = (HWND) lParam;
	COPYDATASTRUCT cds;
	cds.dwData = sendLoc;
	size_t strLen;
	cds.lpData = filePath;
	StringCbLength(filePath, 512, &strLen);
	cds.cbData = strLen;
	//now technically we are finding the HWND each time we do a load
	//but since this is not speed critical I'm not worried.
	//if it is an issue we can pull this out into a static var
	SendMessage(hwnd, WM_COPYDATA, (WPARAM) NULL, (LPARAM) &cds);
}

void LoadToLPCALC(LPARAM lParam, LPTSTR filePath, SEND_FLAG sendLoc)
{
	LPCALC lpCalc = (LPCALC) lParam;
	SendFileToCalc(lpCalc, filePath, TRUE, sendLoc);
}

void LoadCommandlineFiles(ParsedCmdArgs *parsedArgs, LPARAM lParam,  void (*load_callback)(LPARAM, LPTSTR, SEND_FLAG))
{
	//load ROMs first
	for (int i = 0; i < parsedArgs->num_rom_files; i++) {
		load_callback(lParam, parsedArgs->rom_files[i], SEND_ARC);
	}
	//then archived files
	for (int i = 0; i < parsedArgs->num_archive_files; i++) {
		load_callback(lParam, parsedArgs->archive_files[i], SEND_ARC);
	}
	//then ram
	for (int i = 0; i < parsedArgs->num_ram_files; i++) {
		load_callback(lParam, parsedArgs->ram_files[i], SEND_RAM);
	}
	//finally utility files (label, break, etc)
	for (int i = 0; i < parsedArgs->num_utility_files; i++) {
		load_callback(lParam, parsedArgs->utility_files[i], SEND_ARC);
	}
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
   LPSTR lpszCmdParam, int nCmdShow)
{
	MSG Msg;
	bool alreadyRunningWabbit = false;
	int i;

	//this is here so we get our load_files_first setting
	new_calc_on_load_files = QueryWabbitKey(_T("load_files_first"));

	ParsedCmdArgs *parsedArgs = ParseCommandLineArgs();

	HWND alreadyRunningHwnd = NULL;
	alreadyRunningHwnd = FindWindow(g_szAppName, NULL);
	alreadyRunningWabbit = check_no_new_process(alreadyRunningHwnd);
	// If there is a setting to load files into a new calc each time and there is a calc already running
	// ask it to create a new core to load into
	if (new_calc_on_load_files && alreadyRunningHwnd) {
		HWND tempHwnd;
		SendMessage(alreadyRunningHwnd, WM_COMMAND, IDM_FILE_NEW, 0);
		for (int i = 9001; i > 0; i--) {
			tempHwnd = FindWindow(g_szAppName, NULL);
			if (tempHwnd != alreadyRunningHwnd)
				break;
		}
		alreadyRunningHwnd = tempHwnd;
	}
	
	if (alreadyRunningWabbit) {
		LoadCommandlineFiles(parsedArgs, (LPARAM) find_existing_lcd(alreadyRunningHwnd), LoadAlreadyExistingWabbit);
		if (parsedArgs->force_focus) {
			SwitchToThisWindow(alreadyRunningHwnd, TRUE);
		}
		exit(0);
	}

	g_hInst = hInstance;

	RegisterWindowClasses();

	// initialize com events
	OleInitialize(NULL);

	// Initialize GDI+.
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	silent_mode = parsedArgs->silent_mode;

	LPCALC lpCalc = calc_slot_new();
	LoadRegistrySettings(lpCalc);

	if (rom_load(lpCalc, lpCalc->rom_path) == TRUE) {
		gui_frame(lpCalc);
	} else {

		BOOL loadedRom = FALSE;
		if (parsedArgs->num_rom_files > 0) {
			for (int i = 0; i < parsedArgs->num_rom_files; i++) {
				if (rom_load(lpCalc, parsedArgs->rom_files[i])) {
					gui_frame(lpCalc);
					loadedRom = TRUE;
					break;
				}
			}
		}
		if (!loadedRom) {
			calc_slot_free(lpCalc);

			if (show_wizard) {
				BOOL wizardError = DoWizardSheet(NULL);
				//save wizard show
				SaveWabbitKey(_T("show_wizard"), REG_DWORD, &show_wizard);
				SaveWabbitKey(_T("rom_path"), REG_SZ, &lpCalc->rom_path);
				if (wizardError)
					return EXIT_FAILURE;
				LoadRegistrySettings(lpCalc);
				gui_frame(lpCalc);
			} else {
				const TCHAR lpstrFilter[] 	= _T("Known types ( *.sav; *.rom) \0*.sav;*.rom\0\
													Save States  (*.sav)\0*.sav\0\
													ROMs  (*.rom)\0*.rom\0\
													All Files (*.*)\0*.*\0\0");
				const TCHAR lpstrTitle[] = _T("Wabbitemu: Please select a ROM or save state");
				const TCHAR lpstrDefExt[] = _T("rom");
				TCHAR* FileName = (TCHAR *) malloc(MAX_PATH);
				ZeroMemory(FileName, MAX_PATH);
				if (!BrowseFile(FileName, lpstrFilter, lpstrTitle, lpstrDefExt, OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST)) {
					lpCalc = calc_slot_new();
					if (rom_load(lpCalc, FileName) == TRUE)
						gui_frame(lpCalc);
					else return EXIT_FAILURE;
				} else return EXIT_FAILURE;
			}
		}
	}

	StringCbCopy(lpCalc->labelfn, sizeof(lpCalc->labelfn), _T("labels.lab"));

	state_build_applist(&lpCalc->cpu, &lpCalc->applist);
	VoidLabels(lpCalc);
	LoadCommandlineFiles(parsedArgs, (LPARAM) lpCalc, LoadToLPCALC);

	//initialize linking hub
	memset(link_hub, 0, sizeof(link_hub));
	link_t *hub_link = (link_t *) malloc(sizeof(link_t)); 
	if (!hub_link) {
		printf("Couldn't allocate memory for link hub\n");
	}
	hub_link->host		= 0;			//neither lines set
	hub_link->client	= &hub_link->host;	//nothing plugged in.
	link_hub[MAX_CALCS] = hub_link;

#ifdef WITH_AVI
	is_recording = FALSE;
#endif

	InitCommonControls();
	// Set the one global timer for all calcs
	SetTimer(NULL, 0, TPF, TimerProc);

	hacceldebug = LoadAccelerators(g_hInst, _T("DisasmAccel"));
	if (!haccelmain)
		haccelmain = LoadAccelerators(g_hInst, _T("Z80Accel"));

	extern HWND hPortMon, hBreakpoints;
	while (GetMessage(&Msg, NULL, 0, 0)) {
		HACCEL haccel = haccelmain;
		HWND hwndtop = GetForegroundWindow();
		if (hwndtop) {
			if (hwndtop == FindWindow(g_szDebugName, NULL) ) {
				haccel = hacceldebug;
			} else if (hwndtop == FindWindow(g_szAppName, NULL) ) {
				haccel = haccelmain;
				if (lpCalc->bCutout && lpCalc->SkinEnabled) {
					hwndtop = FindWindow(g_szLCDName, NULL);
				} else {
					hwndtop = FindWindowEx(hwndtop, NULL, g_szLCDName, NULL);
				}
				SetForegroundWindow(hwndtop);
			} else if (lpCalc->bCutout && lpCalc->SkinEnabled) {
				if (hwndtop == FindWindow(g_szLCDName, NULL) || hwndtop == FindWindow(g_szAppName, NULL) ||
					hwndtop == FindWindow(g_szSmallButtonsName, g_szSmallClose) ||
					hwndtop == FindWindow(g_szSmallButtonsName, g_szSmallMinimize)) {
					hwndtop = FindWindow(g_szLCDName, NULL);
				} else {
					haccel = NULL;	
				}
			} else {
				haccel = NULL;
			}
		}

		if (hwndProp != NULL) {
			if (PropSheet_GetCurrentPageHwnd(hwndProp) == NULL) {
				GetWindowRect(hwndProp, &PropRect);
				DestroyWindow(hwndProp);
				hwndProp = NULL;
			}
		}

		if (hwndProp == NULL || PropSheet_IsDialogMessage(hwndProp, &Msg) == FALSE) {
			if (!TranslateAccelerator(hwndtop, haccel, &Msg)) {
				TranslateMessage(&Msg);
				DispatchMessage(&Msg);
			}
		} else {
			// Get the current tab
			HWND hwndPropTabCtrl = PropSheet_GetTabControl(hwndProp);
			PropPageLast = TabCtrl_GetCurSel(hwndPropTabCtrl);
		}
	}

	// Make sure the GIF has terminated
	if (gif_write_state == GIF_FRAME) {
		gif_write_state = GIF_END;
		handle_screenshot();
	}

	//free the link we setup to act as our hub
	free(hub_link);
	
	// Shutdown GDI+
	GdiplusShutdown(gdiplusToken);

	// Shutdown COM
	OleUninitialize();
#if _DEBUG
	_CrtDumpMemoryLeaks();
#endif

	return (int) Msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam) {
	//static HDC hdcKeymap;
	static POINT ctxtPt;
	LPCALC lpCalc = (LPCALC) GetWindowLongPtr(hwnd, GWLP_USERDATA);

	switch (Message) {
		case WM_CREATE: {
			LPCALC lpCalc = (LPCALC) ((LPCREATESTRUCT) lParam)->lpCreateParams;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) lpCalc);

			//RegisterDropWindow(hwnd, (IDropTarget **) &lpCalc->pDropTarget);

			// Force the current skin setting to be enacted
			lpCalc->SkinEnabled = !lpCalc->SkinEnabled;
			SendMessage(hwnd, WM_COMMAND, IDM_VIEW_SKIN, 0);
			return 0;
		}
		case WM_USER:
			gui_frame_update(lpCalc);
			break;
		case WM_PAINT: {
#define GIFGRAD_PEAK 15
#define GIFGRAD_TROUGH 10

			static int GIFGRADWIDTH = 1;
			static int GIFADD = 1;

			if (gif_anim_advance) {
				switch (lpCalc->gif_disp_state) {
					case GDS_STARTING:
						if (GIFGRADWIDTH > 15) {
							lpCalc->gif_disp_state = GDS_RECORDING;
							GIFADD = -1;
						} else {
							GIFGRADWIDTH ++;
						}
						break;
					case GDS_RECORDING:
						GIFGRADWIDTH += GIFADD;
						if (GIFGRADWIDTH > GIFGRAD_PEAK) GIFADD = -1;
						else if (GIFGRADWIDTH < GIFGRAD_TROUGH) GIFADD = 1;
						break;
					case GDS_ENDING:
						if (GIFGRADWIDTH) GIFGRADWIDTH--;
						else {
							lpCalc->gif_disp_state = GDS_IDLE;							
							gui_frame_update(lpCalc);						
						}						
						break;
					case GDS_IDLE:
						break;
				}
				gif_anim_advance = FALSE;
			}

			if (lpCalc->gif_disp_state != GDS_IDLE) {
				RECT screen, rc;
				GetWindowRect(lpCalc->hwndLCD, &screen);
				GetWindowRect(lpCalc->hwndFrame, &rc);
				int orig_w = screen.right - screen.left;
				int orig_h = screen.bottom - screen.top;

				AdjustWindowRect(&screen, WS_CAPTION, FALSE);
				screen.top -= GetSystemMetrics(SM_CYMENU);

				//printf("screen: %d\n", screen.left - rc.left);

				SetRect(&screen, screen.left - rc.left -5, screen.top - rc.top - 5,
						screen.left - rc.left + orig_w - 5,
						screen.top - rc.top + orig_h - 5);

				int grayred = (int) (((double) GIFGRADWIDTH / GIFGRAD_PEAK) * 50);
				HDC hWindow = GetDC(hwnd);
				DrawGlow(lpCalc->hdcSkin, hWindow, &screen, RGB(127 - grayred, 127 - grayred, 127 + grayred),
							GIFGRADWIDTH, lpCalc->SkinEnabled);				
				ReleaseDC(hwnd, hWindow);
				InflateRect(&screen, GIFGRADWIDTH, GIFGRADWIDTH);
				ValidateRect(hwnd, &screen);
			}

			PAINTSTRUCT ps;
			HDC hdc;
			hdc = BeginPaint(hwnd, &ps);
			if (lpCalc->SkinEnabled) {
				BitBlt(hdc, 0, 0, lpCalc->rectSkin.right, lpCalc->rectSkin.bottom, lpCalc->hdcButtons, 0, 0, SRCCOPY);
				BitBlt(lpCalc->hdcButtons, 0, 0, lpCalc->rectSkin.right, lpCalc->rectSkin.bottom, lpCalc->hdcSkin, 0, 0, SRCCOPY);
			} else {
				RECT rc;
				GetClientRect(lpCalc->hwndFrame, &rc);
				FillRect(hdc, &rc, GetStockBrush(GRAY_BRUSH));
			}
			ReleaseDC(hwnd, hdc);
			EndPaint(hwnd, &ps);

			return 0;
		}
		case WM_COMMAND: {
			switch (LOWORD(wParam)) {
				case IDM_FILE_NEW: {
						LPCALC lpCalcNew = calc_slot_new();
						if (rom_load(lpCalcNew, lpCalc->rom_path) || rom_load(lpCalcNew, (LPCTSTR) QueryWabbitKey(_T("rom_path")))) {
							lpCalcNew->SkinEnabled = lpCalc->SkinEnabled;
							lpCalcNew->bCutout = lpCalc->bCutout;
							lpCalcNew->scale = lpCalc->scale;
							lpCalcNew->FaceplateColor = lpCalc->FaceplateColor;
							lpCalcNew->bAlphaBlendLCD = lpCalc->bAlphaBlendLCD;

							calc_turn_on(lpCalcNew);
							gui_frame(lpCalcNew);
						} else {
							calc_slot_free(lpCalcNew);
							SendMessage(hwnd, WM_COMMAND, IDM_HELP_WIZARD, 0);
						}
						break;
				}
				case IDM_FILE_OPEN: {
					GetOpenSendFileName(hwnd);
					SetWindowText(hwnd, _T("Wabbitemu"));
					break;
				}
				case IDM_FILE_SAVE: {
					TCHAR FileName[MAX_PATH];
					const TCHAR lpstrFilter[] = _T("Known File types ( *.sav; *.rom; *.bin) \0*.sav;*.rom;*.bin\0\
														Save States  (*.sav)\0*.sav\0\
														ROMS  (*.rom; .bin)\0*.rom;*.bin\0\
														OSes (*.8xu)\0*.8xu\0\
														All Files (*.*)\0*.*\0\0");
					ZeroMemory(FileName, MAX_PATH);
					if (!SaveFile(FileName, (TCHAR *) lpstrFilter, _T("Wabbitemu Save State"), _T("sav"), OFN_PATHMUSTEXIST)) {
						TCHAR extension[5] = _T("");
						const TCHAR *pext = _tcsrchr(FileName, _T('.'));
						if (pext != NULL)
						{
							StringCbCopy(extension, sizeof(extension), pext);
						}
						if (!_tcsicmp(extension, _T(".rom")) || !_tcsicmp(extension, _T(".bin"))) {
							MFILE *file = ExportRom(FileName, lpCalc);
							StringCbCopy(lpCalc->rom_path, sizeof(lpCalc->rom_path), FileName);
							mclose(file);
						} else if (!_tcsicmp(extension, _T(".8xu"))) {
							HWND hExportOS = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_EXPORT_OS), hwnd, (DLGPROC) ExportOSDialogProc, (LPARAM) FileName);
							ShowWindow(hExportOS, SW_SHOW);
						} else {
							SAVESTATE_t *save = SaveSlot(lpCalc);
							gui_savestate(hwnd, save, FileName, lpCalc);
						}
					}
					break;
				}
				case IDM_FILE_GIF: {
					HMENU hmenu = GetMenu(hwnd);
					if (gif_write_state == GIF_IDLE) {
						BOOL start_screenshot = get_gif_filename();
						if (!start_screenshot)
							break;
						gif_write_state = GIF_START;
						for (int i = 0; i < MAX_CALCS; i++)
							if (calcs[i].active)
								calcs[i].gif_disp_state = GDS_STARTING;
						CheckMenuItem(GetSubMenu(hmenu, MENU_FILE), IDM_FILE_GIF, MF_BYCOMMAND | MF_CHECKED);
					} else {
						gif_write_state = GIF_END;
						for (int i = 0; i < MAX_CALCS; i++)
							if (calcs[i].active)
								calcs[i].gif_disp_state = GDS_ENDING;
						CheckMenuItem(GetSubMenu(hmenu, MENU_FILE), IDM_FILE_GIF, MF_BYCOMMAND | MF_UNCHECKED);
					}
					break;
				}
				case IDM_FILE_STILLGIF: {
					BOOL start_screenshot = get_gif_filename();
					if (start_screenshot) {
						LCD_t *lcd = lpCalc->cpu.pio.lcd;
						gif_xs = lcd->width * gif_size;
						gif_ys = SCRYSIZE * gif_size;
						GIFGREYLCD(lcd);

						unsigned int i, j;
						for (i = 0; i < SCRYSIZE * gif_size; i++)
							for (j = 0; j < lcd->width * gif_size; j++)
								gif_frame[i * gif_xs + j] = lpCalc->cpu.pio.lcd->gif[i][j];

						gif_write_state = GIF_START;
						gif_writer(lcd->shades);

						gif_write_state = GIF_END;
						gif_writer(lcd->shades);
					}
					break;
				}
				case IDM_FILE_AVI: {
					HMENU hmenu = GetMenu(hwnd);
#ifdef WITH_AVI
					if (is_recording) {
						CloseAvi(recording_avi);
						is_recording = FALSE;
						CheckMenuItem(GetSubMenu(hmenu, MENU_FILE), IDM_FILE_AVI, MF_BYCOMMAND | MF_UNCHECKED);
					} else {
						TCHAR lpszFile[MAX_PATH];
						if (!SaveFile(lpszFile, _T("AVIs (*.avi)\0*.avi\0All Files (*.*)\0*.*\0\0"),
											_T("Wabbitemu Export AVI"), _T("avi"), OFN_PATHMUSTEXIST)) {
							recording_avi = CreateAvi(lpszFile, FPS, NULL);
							//create an initial first frame so we can set compression
							is_recording = TRUE;
							/*AVICOMPRESSOPTIONS opts;
							ZeroMemory(&opts,sizeof(opts));
							opts.fccHandler = mmioFOURCC('d','i','v','x');
							SetAviVideoCompression(recording_avi, hbm, &opts, true, hwnd);*/
							CheckMenuItem(GetSubMenu(hmenu, MENU_FILE), IDM_FILE_AVI, MF_BYCOMMAND | MF_CHECKED);
						}
					}
#endif
					break;
				}
				case IDM_FILE_CLOSE:
					return SendMessage(hwnd, WM_CLOSE, 0, 0);
				case IDM_FILE_EXIT:
					if (calc_count() > 1) {
						TCHAR buf[256];
						StringCbPrintf(buf, sizeof(buf), _T("If you exit now, %d other running calculator(s) will be closed. \
															Are you sure you want to exit?"), calc_count() - 1);
						int res = MessageBox(NULL, buf, _T("Wabbitemu"), MB_YESNO);
						if (res == IDCANCEL || res == IDNO)
							break;
						is_exiting = TRUE;
					}
					PostQuitMessage(0);
					break;
				case IDM_CALC_COPY: {
					HLOCAL ans;
					ans = (HLOCAL) GetRealAns(&lpCalc->cpu);
					OpenClipboard(hwnd);
					EmptyClipboard();
					SetClipboardData(CF_TEXT, ans);
					CloseClipboard();
					break;
				}
				case IDM_EDIT_PASTE: {
					
					break;
				}
				case IDM_VIEW_SKIN: {
					lpCalc->SkinEnabled = !lpCalc->SkinEnabled;
					gui_frame_update(lpCalc);
					break;
				}
				case IDM_VIEW_LCD: {
					if (lpCalc->hwndDetachedLCD || lpCalc->hwndDetachedFrame) {
						break;
					}
					RECT r;
					SetRect(&r, 0, 0, (lpCalc->rectLCD.right - lpCalc->rectLCD.left) / 2 * lpCalc->scale, 64 * lpCalc->scale);
					AdjustWindowRect(&r, WS_CAPTION | WS_TILEDWINDOW, FALSE);

					lpCalc->hwndDetachedFrame  = CreateWindowEx(
						0,
						g_szDetachedName,
						_T("Z80"),
						(WS_TILEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN) & ~(WS_MAXIMIZEBOX | WS_MINIMIZEBOX),
						startX, startY, r.right - r.left, r.bottom - r.top,
						NULL, 0, g_hInst, (LPVOID) lpCalc);

					SetWindowText(lpCalc->hwndDetachedFrame, _T("LCD"));

					if (lpCalc->hwndDetachedFrame == NULL) {
						return -1;
					}

					break;
				}
				case IDM_CALC_SOUND: {
					togglesound(lpCalc->audio);
					CheckMenuItem(GetSubMenu(GetMenu(hwnd), 2), IDM_CALC_SOUND, MF_BYCOMMAND | (lpCalc->audio->enabled ? MF_CHECKED : MF_UNCHECKED));
					break;
				}
				case ID_DEBUG_TURNONCALC:
					{
						calc_turn_on(lpCalc);
						break;
					}
				case IDM_CALC_CONNECT: {
					/*if (!calcs[0].active || !calcs[1].active || link_connect(&calcs[0].cpu, &calcs[1].cpu))						
						MessageBox(NULL, _T("Connection Failed"), _T("Error"), MB_OK);					
					else*/
					link_connect_hub(lpCalc->slot, &lpCalc->cpu);
					TCHAR buf[64];
					StringCbCopy(buf, sizeof(buf), CalcModelTxt[lpCalc->model]);
					StringCbCat(buf, sizeof(buf), _T(" Connected"));
					SendMessage(lpCalc->hwndStatusBar, SB_SETTEXT, 1, (LPARAM) buf);
					StringCbPrintf(buf, sizeof(buf), _T("Wabbitemu (%d)"), lpCalc->slot + 1);
					SetWindowText(hwnd, buf);			
					break;
				}
				case IDM_CALC_PAUSE: {
					HMENU hmenu = GetMenu(hwnd);
					if (lpCalc->running) {
						CheckMenuItem(GetSubMenu(hmenu, 2), IDM_CALC_PAUSE, MF_BYCOMMAND | MF_CHECKED);
						lpCalc->running = FALSE;
					} else {
						CheckMenuItem(GetSubMenu(hmenu, 2), IDM_CALC_PAUSE, MF_BYCOMMAND | MF_UNCHECKED);
						lpCalc->running = TRUE;
					}
					break;
				}
				case IDM_VIEW_VARIABLES:
					CreateVarTreeList(hwnd, lpCalc);
					break;
				case IDM_VIEW_KEYSPRESSED:
					if (IsWindow(lpCalc->hwndKeyListDialog)) {
						SwitchToThisWindow(lpCalc->hwndKeyListDialog, TRUE);
					} else {
						lpCalc->hwndKeyListDialog = (HWND) CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_KEYS_LIST), hwnd, (DLGPROC) KeysListProc);
						ShowWindow(lpCalc->hwndKeyListDialog, SW_SHOW);
					}
					break;
				case IDM_CALC_OPTIONS:
					DoPropertySheet(hwnd);
					break;
				case IDM_DEBUG_RESET: {
					calc_reset(lpCalc);
					break;
				}
				case IDM_DEBUG_OPEN:
					gui_debug(lpCalc);
					break;
				case IDM_HELP_ABOUT:
					lpCalc->running = FALSE;
					DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DLGABOUT), hwnd, (DLGPROC) AboutDialogProc);					
					lpCalc->running = TRUE;
					break;
				case IDM_HELP_WIZARD:
					DoWizardSheet(hwnd);
					break;
				case IDM_HELP_WEBSITE:					
					ShellExecute(NULL, _T("open"), g_szWebPage, NULL, NULL, SW_SHOWNORMAL);
					break;
				case IDM_FRAME_BTOGGLE:
					SendMessage(hwnd, WM_MBUTTONDOWN, MK_MBUTTON, MAKELPARAM(ctxtPt.x, ctxtPt.y));
					break;
				case IDM_FRAME_BUNLOCK: {
					RECT rc;
					keypad_t *kp = (keypad_t *) lpCalc->cpu.pio.devices[1].aux;
					int group, bit;
					GetClientRect(hwnd, &rc);
					for(group = 0; group < 7; group++) {
						for(bit = 0; bit < 8; bit++) {
							kp->keys[group][bit] &= (~KEY_LOCKPRESS);
						}
					}
					lpCalc->cpu.pio.keypad->on_pressed &= (~KEY_LOCKPRESS);

					FinalizeButtons(lpCalc);
					break;
				}
				case IDM_SPEED_QUARTER: {
					lpCalc->speed = 25;
					HMENU hmenu = GetMenu(hwnd);
					CheckMenuRadioItem(GetSubMenu(hmenu, 2), IDM_SPEED_QUARTER, IDM_SPEED_SET, IDM_SPEED_QUARTER, MF_BYCOMMAND | MF_CHECKED);
					break;
				}
				case IDM_SPEED_HALF: {
					lpCalc->speed = 50;
					HMENU hmenu = GetMenu(hwnd);
					CheckMenuRadioItem(GetSubMenu(hmenu, 2), IDM_SPEED_QUARTER, IDM_SPEED_SET, IDM_SPEED_HALF, MF_BYCOMMAND | MF_CHECKED);
					break;
				}
				case IDM_SPEED_NORMAL: {
					lpCalc->speed = 100;
					HMENU hmenu = GetMenu(hwnd);
					CheckMenuRadioItem(GetSubMenu(hmenu, 2), IDM_SPEED_QUARTER, IDM_SPEED_SET, IDM_SPEED_NORMAL, MF_BYCOMMAND | MF_CHECKED);
					break;
				}
				case IDM_SPEED_DOUBLE: {
					lpCalc->speed = 200;
					HMENU hmenu = GetMenu(hwnd);
					CheckMenuRadioItem(GetSubMenu(hmenu, 2), IDM_SPEED_QUARTER, IDM_SPEED_SET, IDM_SPEED_DOUBLE, MF_BYCOMMAND | MF_CHECKED);
					break;
				}
				case IDM_SPEED_QUADRUPLE: {
					lpCalc->speed = 400;
					HMENU hmenu = GetMenu(hwnd);
					CheckMenuRadioItem(GetSubMenu(hmenu, 2), IDM_SPEED_QUARTER, IDM_SPEED_SET, IDM_SPEED_QUADRUPLE, MF_BYCOMMAND | MF_CHECKED);
					break;
				}
				case IDM_SPEED_MAX: {
					lpCalc->speed = MAX_SPEED;
					HMENU hmenu = GetMenu(hwnd);
					CheckMenuRadioItem(GetSubMenu(hmenu, 2), IDM_SPEED_QUARTER, IDM_SPEED_SET, IDM_SPEED_MAX, MF_BYCOMMAND | MF_CHECKED);
					break;
				}
				case IDM_SPEED_SET: {
					int dialog = DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_DLGSPEED), hwnd, (DLGPROC) SetSpeedProc, (LPARAM) lpCalc);
					if (dialog == IDOK) {
						HMENU hMenu = GetMenu(hwnd);
						switch(lpCalc->speed)
						{
							case 25:
								CheckMenuRadioItem(hMenu, IDM_SPEED_QUARTER, IDM_SPEED_SET, IDM_SPEED_QUARTER, MF_BYCOMMAND| MF_CHECKED);
								break;
							case 50:
								CheckMenuRadioItem(hMenu, IDM_SPEED_QUARTER, IDM_SPEED_SET, IDM_SPEED_HALF, MF_BYCOMMAND| MF_CHECKED);
								break;
							case 100:
								CheckMenuRadioItem(hMenu, IDM_SPEED_QUARTER, IDM_SPEED_SET, IDM_SPEED_NORMAL, MF_BYCOMMAND| MF_CHECKED);
								break;
							case 200:
								CheckMenuRadioItem(hMenu, IDM_SPEED_QUARTER, IDM_SPEED_SET, IDM_SPEED_DOUBLE, MF_BYCOMMAND| MF_CHECKED);
								break;
							case 400:
								CheckMenuRadioItem(hMenu, IDM_SPEED_QUARTER, IDM_SPEED_SET, IDM_SPEED_QUADRUPLE, MF_BYCOMMAND| MF_CHECKED);
								break;
							default:
								CheckMenuRadioItem(hMenu, IDM_SPEED_QUARTER, IDM_SPEED_SET, IDM_SPEED_SET, MF_BYCOMMAND| MF_CHECKED);
								break;
						}
					}
					SetFocus(hwnd);
					break;
				}
				case IDM_HELP_UPDATE: {
					TCHAR buffer[MAX_PATH];
					TCHAR *env;
					size_t envLen;
					_tdupenv_s(&env, &envLen, _T("appdata"));
					StringCbCopy(buffer, sizeof(buffer), env);
					free(env);
					StringCbCat(buffer, sizeof(buffer), _T("\\Revsoft.Autoupdater.exe"));
					HRSRC hrDumpProg = FindResource(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_UPDATER), _T("EXE"));
					ExtractResource(buffer, hrDumpProg);

					TCHAR argBuf[MAX_PATH * 3];
					TCHAR filePath[MAX_PATH];
					GetModuleFileName(NULL, filePath, MAX_PATH);
					StringCbPrintf(argBuf, sizeof(argBuf), _T("\"%s\" -R \"%s\" \"%s\" \"%s\""), buffer, filePath, filePath, g_szDownload);
					STARTUPINFO si;
					PROCESS_INFORMATION pi;
					memset(&si, 0, sizeof(si)); 
					memset(&pi, 0, sizeof(pi)); 
					si.cb = sizeof(si);
					MessageBox(NULL, argBuf, _T("TEST"), MB_OK);
					if (!CreateProcess(NULL, argBuf,
						NULL, NULL, FALSE, CREATE_DEFAULT_ERROR_MODE, 
						NULL, NULL, &si, &pi)) {
						MessageBox(NULL, _T("Unable to start the process. Try manually downloading the update."), _T("Error"), MB_OK);
						return FALSE;
					}
					exit(0);
					break;
				}
			}
			/*switch (HIWORD(wParam)) {
			}*/
			return 0;
		}
		//case WM_MOUSEMOVE:
		case WM_LBUTTONUP:
		{
			int group, bit;
			static POINT pt;
			BOOL repostMessage = FALSE;
			keypad_t *kp = lpCalc->cpu.pio.keypad;

			ReleaseCapture();
#define KEY_TIMER 1
			KillTimer(hwnd, KEY_TIMER);

			for (group = 0; group < 7; group++) {
				for (bit = 0; bit < 8; bit++) {
#define MIN_KEY_DELAY 400
					if (kp->last_pressed[group][bit] - lpCalc->cpu.timer_c->tstates >= MIN_KEY_DELAY || !lpCalc->running) {
						kp->keys[group][bit] &= (~KEY_MOUSEPRESS);
					} else {
						repostMessage = TRUE;
					}
				}
			}

			if (kp->on_last_pressed - lpCalc->cpu.timer_c->tstates >= MIN_KEY_DELAY || !lpCalc->running) {
				lpCalc->cpu.pio.keypad->on_pressed &= ~KEY_MOUSEPRESS;
			} else {
				repostMessage = TRUE;
			}

			if (repostMessage) {
				SetTimer(hwnd, KEY_TIMER, 50, NULL);
			}

			FinalizeButtons(lpCalc);
			return 0;
		}
		case WM_LBUTTONDOWN:
		{
			int group, bit;
			static POINT pt;
			keypad_t *kp = lpCalc->cpu.pio.keypad;

			SetCapture(hwnd);
			pt.x	= GET_X_LPARAM(lParam);
			pt.y	= GET_Y_LPARAM(lParam);
			if (lpCalc->bCutout) {
				pt.y += GetSystemMetrics(SM_CYCAPTION);	
				pt.x += GetSystemMetrics(SM_CXSIZEFRAME);
			}

			for (group = 0; group < 7; group++) {
				for (bit = 0; bit < 8; bit++) {
					kp->keys[group][bit] &= (~KEY_MOUSEPRESS);
				}
			}

			kp->on_pressed &= ~KEY_MOUSEPRESS;

			COLORREF c = GetPixel(lpCalc->hdcKeymap, pt.x, pt.y);
			if (GetRValue(c) == 0xFF) {
				FinalizeButtons(lpCalc);
				return 0;
			}

			group = GetGValue(c) >> 4;
			bit	= GetBValue(c) >> 4;
			LogKeypress(lpCalc, group, bit);
			if (group == 0x05 && bit == 0x00){
				kp->on_pressed |= KEY_MOUSEPRESS;
				kp->on_last_pressed = lpCalc->cpu.timer_c->tstates;
			} else {
				kp->keys[group][bit] |= KEY_MOUSEPRESS;
				if ((kp->keys[group][bit] & KEY_STATEDOWN) == 0) {
					kp->keys[group][bit] |= KEY_STATEDOWN;
					kp->last_pressed[group][bit] = lpCalc->cpu.timer_c->tstates;
				}
			}
			FinalizeButtons(lpCalc);
			return 0;
		}
		case WM_TIMER: {
			if (wParam == KEY_TIMER) {
				PostMessage(hwnd, WM_LBUTTONUP, 0, 0);
			}
			break;
		}
		case WM_MBUTTONDOWN: {
			int group,bit;
			POINT pt;
			keypad_t *kp = (keypad_t *) (&lpCalc->cpu)->pio.devices[1].aux;

			pt.x	= GET_X_LPARAM(lParam);
			pt.y	= GET_Y_LPARAM(lParam);
			if (lpCalc->bCutout) {
				pt.y += GetSystemMetrics(SM_CYCAPTION);	
				pt.x += GetSystemMetrics(SM_CXSIZEFRAME);
			}

			COLORREF c = GetPixel(lpCalc->hdcKeymap, pt.x, pt.y);
			if (GetRValue(c) == 0xFF) return 0;
			group	= GetGValue(c) >> 4;
			bit		= GetBValue(c) >> 4;

			if (group== 0x05 && bit == 0x00) {
				lpCalc->cpu.pio.keypad->on_pressed ^= KEY_LOCKPRESS;
			} else {
				kp->keys[group][bit] ^= KEY_LOCKPRESS;
			}
			FinalizeButtons(lpCalc);
			return 0;
		}

		case WM_KEYDOWN: {
			HandleKeyDown(lpCalc, wParam);
			return 0;
		}
		case WM_KEYUP:
			if (wParam) {
				HandleKeyUp(lpCalc, wParam);
			}
			return 0;
		case WM_SIZING: {
			if (lpCalc->SkinEnabled) {
				return TRUE;
			}
			RECT *prc = (RECT *) lParam;
			LONG ClientAdjustWidth, ClientAdjustHeight;
			LONG AdjustWidth, AdjustHeight;

			// Adjust for border and menu
			RECT rc = {0, 0, 0, 0};
			AdjustWindowRect(&rc, WS_CAPTION | WS_TILEDWINDOW, FALSE);
			if (GetMenu(hwnd) != NULL)
			{
				rc.bottom += GetSystemMetrics(SM_CYMENU);
			}

			RECT src;
			if (lpCalc->hwndStatusBar != NULL) {
				GetWindowRect(lpCalc->hwndStatusBar, &src);
				rc.bottom += src.bottom - src.top;
			}
			//don't allow resizing from the sides
			if (wParam == WMSZ_LEFT || wParam == WMSZ_RIGHT 
				|| wParam == WMSZ_TOP || wParam == WMSZ_BOTTOM) {
					GetWindowRect(hwnd, &rc);
					memcpy(prc, &rc, sizeof(RECT));
					return TRUE;
			}

			ClientAdjustWidth = rc.right - rc.left;
			ClientAdjustHeight = rc.bottom - rc.top;


			switch (wParam) {
			case WMSZ_BOTTOMLEFT:
			case WMSZ_LEFT:
			case WMSZ_TOPLEFT:
				prc->left -= 128 / 4;
				break;
			default:
				prc->right += 128 / 4;
				break;
			}

			switch (wParam) {
			case WMSZ_TOPLEFT:
			case WMSZ_TOP:
			case WMSZ_TOPRIGHT:
				prc->top -= 64 / 4;
				break;
			default:
				prc->bottom += 64 / 4;
				break;
			}


			// Make sure the width is a nice clean proportional sizing
			AdjustWidth = (prc->right - prc->left - ClientAdjustWidth) % 128;
			AdjustHeight = (prc->bottom - prc->top - ClientAdjustHeight) % 64;

			int cx_mult = (prc->right - prc->left - ClientAdjustWidth) / 128;
			int cy_mult = (prc->bottom - prc->top - ClientAdjustHeight) / 64;

			while (cx_mult < 2 || cy_mult < 2) {
				if (cx_mult < 2) {cx_mult++; AdjustWidth -= 128;}
				if (cy_mult < 2) {cy_mult++; AdjustHeight -= 64;}
			}

			if (cx_mult > cy_mult) {
				AdjustWidth += (cx_mult - cy_mult) * 128;
			} else if (cy_mult > cx_mult) {
				AdjustHeight += (cy_mult - cx_mult) * 64;
			}


			lpCalc->scale = min(cx_mult, cy_mult);

			switch (wParam) {
			case WMSZ_BOTTOMLEFT:
			case WMSZ_LEFT:
			case WMSZ_TOPLEFT:
				prc->left += AdjustWidth;
				break;
			default:
				prc->right -= AdjustWidth;
				break;
			}

			switch (wParam) {
			case WMSZ_TOPLEFT:
			case WMSZ_TOP:
			case WMSZ_TOPRIGHT:
				prc->top += AdjustHeight;
				break;
			default:
				prc->bottom -= AdjustHeight;
				break;
			}
			RECT rect;
			GetClientRect(hwnd, &rect);
			InvalidateRect(hwnd, &rect, TRUE);
			return TRUE;
		}
		case WM_SIZE: {
			RECT rc, screen;
			GetClientRect(hwnd, &rc);
			HMENU hmenu = GetMenu(hwnd);
			int cyMenu = hmenu == NULL ? 0 : GetSystemMetrics(SM_CYMENU);
			if ((lpCalc->bCutout && lpCalc->SkinEnabled))	
				rc.bottom += cyMenu;
			int desired_height = lpCalc->SkinEnabled ?  lpCalc->rectSkin.bottom : 128;

			int status_height;
			if (lpCalc->hwndStatusBar == NULL) {
				status_height = 0;
			} else {
				RECT src;
				GetWindowRect(lpCalc->hwndStatusBar, &src);

				status_height = src.bottom - src.top;
				desired_height += status_height;
			}

			rc.bottom -= status_height;

			float xc = 1, yc = 1;
			if (!lpCalc->SkinEnabled) {
				xc = ((float) rc.right) / 256.0;
				yc = ((float) rc.bottom) / 128.0;
			}
			int width = lpCalc->rectLCD.right - lpCalc->rectLCD.left;
			SetRect(&screen,
				0, 0,
				(int) (width * xc),
				(int) (64 * 2 * yc));

			if (lpCalc->SkinEnabled)
				OffsetRect(&screen, lpCalc->rectLCD.left, lpCalc->rectLCD.top);
			else
				OffsetRect(&screen, (int) ((rc.right - width * xc) / 2), 0);

			if ((rc.right - rc.left) & 1) rc.right++;
			if ((rc.bottom - rc.top) & 1) rc.bottom++;

			RECT client;
			client.top = 0;
			client.left = 0;
			if (lpCalc->SkinEnabled) {
				if (lpCalc->bCutout) {
					GetWindowRect(hwnd, &client);
				}
				RECT correctSize = lpCalc->rectSkin;
				AdjustWindowRect(&correctSize, (WS_TILEDWINDOW |  WS_VISIBLE | WS_CLIPCHILDREN) & ~(WS_MAXIMIZEBOX), cyMenu);
				if (correctSize.left < 0) {
					correctSize.right -= correctSize.left;
				}
				if (correctSize.top < 0) {
					correctSize.bottom -= correctSize.top;
				}
				SetWindowPos(hwnd, NULL, 0, 0, correctSize.right, correctSize.bottom , SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_DRAWFRAME);
			}
			RECT windowRect;
			GetWindowRect(hwnd, &windowRect);

			if (windowRect.bottom - windowRect.top != screen.bottom - screen.top ||
				windowRect.right - windowRect.left != screen.right - screen.left)
			{
				MoveWindow(lpCalc->hwndLCD, screen.left + client.left, screen.top + client.top,
					screen.right-screen.left, screen.bottom-screen.top, FALSE);
			}
			ValidateRect(hwnd, &screen);
			//printf("screen: %d\n", screen.right - screen.left);
			if (lpCalc->hwndStatusBar != NULL)
				SendMessage(lpCalc->hwndStatusBar, WM_SIZE, 0, 0);

			//force little buttons to be correct
			PositionLittleButtons(hwnd);
			UpdateWindow(lpCalc->hwndLCD);
			//InvalidateRect(hwnd, NULL, FALSE);
			return 0;
		}
		//case WM_MOVING:
		case WM_MOVE: {
			if (lpCalc->bCutout && lpCalc->SkinEnabled) {
				HDWP hdwp = BeginDeferWindowPos(3);
				RECT rc;
				GetWindowRect(hwnd, &rc);
				OffsetRect(&rc, lpCalc->rectLCD.left, lpCalc->rectLCD.top);
				DeferWindowPos(hdwp, lpCalc->hwndLCD, HWND_TOP, rc.left, rc.top, 0, 0, SWP_NOSIZE);
				EndDeferWindowPos(hdwp);
				PositionLittleButtons(hwnd);
			}
			return 0;
		}
		case WM_CONTEXTMENU: {
			ctxtPt.x = GET_X_LPARAM(lParam);
			ctxtPt.y = GET_Y_LPARAM(lParam);

			HMENU hmenu = LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_FRAME_MENU));
			// TrackPopupMenu cannot display the menu bar so get
			// a handle to the first shortcut menu.
			hmenu = GetSubMenu(hmenu, 0);

			if (!OnContextMenu(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), hmenu)) {
				DefWindowProc(hwnd, Message, wParam, lParam);
			}
			ScreenToClient(hwnd, &ctxtPt);
			DestroyMenu(hmenu);
			return 0;
		}
		case WM_GETMINMAXINFO: {
			if (lpCalc == NULL)
				return 0;
			if (!lpCalc->SkinEnabled)
				break;
			MINMAXINFO *info = (MINMAXINFO *) lParam;
			RECT rc = { 0, 0, SKIN_WIDTH, SKIN_HEIGHT };
			AdjustWindowRect(&rc, WS_CAPTION | WS_TILEDWINDOW, FALSE);
			info->ptMinTrackSize.x = rc.right - rc.left;
			info->ptMinTrackSize.y = rc.bottom - rc.top;
			info->ptMaxTrackSize.x = rc.right - rc.left;
			info->ptMaxTrackSize.y = rc.bottom - rc.top;
			return 0;
		}
		case WM_KILLFOCUS: {
			keypad_t *keypad = lpCalc->cpu.pio.keypad;
			//handle keys already down (just send release)
			//i send the message here so that things like logging are handled
			for (int group = 0; group < 8; group++) {
				for (int bit = 0; bit < 8; bit++) {
					if (keypad->keys[group][bit]) {
						keypad_vk_release(hwnd, group, bit);
					}
				}
			}
			return 0;
		}
		case WM_CLOSE:
			if (calc_count() == 1) {
				if (exit_save_state)
				{
					TCHAR temp_save[MAX_PATH];
					size_t len;
					TCHAR *path;
					_tdupenv_s(&path, &len, _T("appdata"));
					StringCbCopy(temp_save, sizeof(temp_save), path);
					free(path);
					StringCbCat(temp_save, sizeof(temp_save), _T("\\wabbitemu.sav"));
					StringCbCopy(lpCalc->rom_path, sizeof(lpCalc->rom_path), temp_save);
					SAVESTATE_t *save = SaveSlot(lpCalc);
					WriteSave(temp_save, save, true);
					FreeSave(save);
				}

				DestroyCutoutResources();

				SaveRegistrySettings(lpCalc);

			}
			DestroyWindow(hwnd);
			calc_slot_free(lpCalc);
			if (calc_count() == 0)
				PostQuitMessage(0);
			return 0;
		case WM_DESTROY: {
				DeleteDC(lpCalc->hdcKeymap);
				DeleteDC(lpCalc->hdcSkin);
				lpCalc->hdcKeymap = NULL;
				lpCalc->hdcSkin = NULL;

				if (lpCalc->hwndDebug)
					DestroyWindow(lpCalc->hwndDebug);
				lpCalc->hwndDebug = NULL;

				if (lpCalc->hwndStatusBar)
					DestroyWindow(lpCalc->hwndStatusBar);
				lpCalc->hwndStatusBar = NULL;

				if (lpCalc->hwndSmallClose)
					DestroyWindow(lpCalc->hwndSmallClose);
				lpCalc->hwndSmallClose = NULL;

				if (lpCalc->hwndSmallMinimize)
					DestroyWindow(lpCalc->hwndSmallMinimize);
				lpCalc->hwndSmallMinimize = NULL;

				//if (link_connected(lpCalc->slot))
				//	link_disconnect(&lpCalc->cpu);

				lpCalc->hwndFrame = NULL;
				return 0;
			}
		case WM_NCHITTEST:
		{
			int htRet = (int) DefWindowProc(hwnd, Message, wParam, lParam);
			if (htRet != HTCLIENT) return htRet;

			POINT pt;
			pt.x = GET_X_LPARAM(lParam);
			pt.y = GET_Y_LPARAM(lParam);
			if (lpCalc->bCutout && lpCalc->SkinEnabled) {
				pt.y += GetSystemMetrics(SM_CYCAPTION);
				pt.x += GetSystemMetrics(SM_CXFIXEDFRAME);
			}
			ScreenToClient(hwnd, &pt);
			if (GetRValue(GetPixel(lpCalc->hdcKeymap, pt.x, pt.y)) != 0xFF)
				return htRet;
			return HTCAPTION;
		}
		default:
			return DefWindowProc(hwnd, Message, wParam, lParam);
	}
	return 0;
}

INT_PTR CALLBACK AboutDialogProc(HWND hwndDlg, UINT Message, WPARAM wParam, LPARAM lParam) {
	switch (Message) {
		case WM_INITDIALOG:
			return FALSE;
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK:
					EndDialog(hwndDlg, IDOK);
					break;
				case IDCANCEL:
					EndDialog(hwndDlg, IDCANCEL);
					break;
			}
	}
	return FALSE;
}

INT_PTR CALLBACK ExportOSDialogProc(HWND hwndDlg, UINT Message, WPARAM wParam, LPARAM lParam) {
	static HWND hListPagesToExport;
	static LPCALC lpCalc;
	static TCHAR lpFileName[MAX_PATH];
	switch (Message) {
		case WM_INITDIALOG: {
			lpCalc = (LPCALC) GetWindowLongPtr(GetParent(hwndDlg), GWLP_USERDATA);
			StringCbCopy(lpFileName, sizeof(lpFileName), (TCHAR *) lParam);			
			hListPagesToExport = GetDlgItem(hwndDlg, IDC_LIST_EXPORTPAGES);
			SetWindowTheme(hListPagesToExport, L"Explorer", NULL);
			ListView_SetExtendedListViewStyle(hListPagesToExport, LVS_EX_CHECKBOXES);
			TCHAR temp[64];
			int totalPages = lpCalc->cpu.mem_c->flash_pages;
			for (int i = 0; i < totalPages; i++) {
				LVITEM item;
				item.mask = LVIF_TEXT;		
				StringCbPrintf(temp, sizeof(temp), _T("%02X"), i);
				item.pszText = temp;
				item.iItem = i;
				item.iSubItem = 0;
				ListView_InsertItem(hListPagesToExport, &item);
				upages_t pages;
				state_userpages(&lpCalc->cpu, &pages);
				if (i < pages.end - 1|| (i > pages.start && (i & 0xF) != 0xE && (i & 0xF) != 0xF))
				{
					ListView_SetCheckState(hListPagesToExport, i, TRUE);
				}
			}
			return FALSE;
		}
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK: {
					int bufferSize = 0;
					u_char (*flash)[PAGE_SIZE] = (u_char (*)[PAGE_SIZE]) lpCalc->cpu.mem_c->flash;
					unsigned char *buffer = NULL;
					unsigned char *bufferPtr = buffer;
					int currentPage = -1;
					for (int i = 0; i < lpCalc->cpu.mem_c->flash_pages; i++) {
						if (ListView_GetCheckState(hListPagesToExport, i)) {
							bufferSize += PAGE_SIZE;
							unsigned char *new_buffer = (unsigned char *) malloc(bufferSize);
							if (buffer) {
								memcpy(new_buffer, buffer, bufferSize - PAGE_SIZE);
								free(buffer);
							}
							buffer = new_buffer;
							bufferPtr = buffer + bufferSize - PAGE_SIZE;
							memcpy(bufferPtr, flash[i], PAGE_SIZE);
						}
					}
					MFILE *file = ExportOS(lpFileName, buffer, bufferSize);
					mclose(file);
					free(buffer);
					EndDialog(hwndDlg, IDOK);
					return TRUE;
				}
				case IDCANCEL:
					EndDialog(hwndDlg, IDCANCEL);
					break;
			}
	}
	return FALSE;
}
