#include "stdafx.h"

#include "gui.h"
#include "SendFiles.h"
#include "CWabbitemu.h"
#include "CPage.h"

#define WM_ADDFRAME	(WM_USER+5)
#define WM_REMOVEFRAME (WM_USER+6)

DWORD CWabbitemu::m_dwThreadId = 0;

STDMETHODIMP CWabbitemu::QueryInterface(REFIID riid, LPVOID *ppvObject)
{
	if (riid == IID_IUnknown)
	{
		this->AddRef();
		*ppvObject = this;
		return S_OK;
	}
	else if (riid == IID_IWabbitemu)
	{
		this->AddRef();
		*ppvObject = this;
		return S_OK;
	}
	else
	{
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}
}

STDMETHODIMP CWabbitemu::put_Visible(VARIANT_BOOL fVisible)
{
	if (fVisible == VARIANT_TRUE)
	{
		PostThreadMessage(m_dwThreadId, WM_ADDFRAME, m_iSlot, (LPARAM) m_lpCalc);
	}
	else
	{
		PostThreadMessage(m_dwThreadId, WM_REMOVEFRAME, m_iSlot, (LPARAM) m_lpCalc);
	}
	return S_OK;
}

STDMETHODIMP CWabbitemu::get_Visible(VARIANT_BOOL *lpVisible)
{
	*lpVisible = m_fVisible;
	return S_OK;
}

STDMETHODIMP CWabbitemu::get_CPU(IZ80 **ppZ80)
{
	return m_pZ80->QueryInterface(IID_IZ80,(LPVOID *) ppZ80);
}

STDMETHODIMP CWabbitemu::get_LCD(ILCD **ppLCD)
{
	return m_pLCD->QueryInterface(IID_ILCD,(LPVOID *) ppLCD);
}

STDMETHODIMP CWabbitemu::Step()
{
	CPU_step(&m_lpCalc->cpu);
	return S_OK;
}

STDMETHODIMP CWabbitemu::StepOver()
{
	//CPU_stepover(&m_lpCalc->cpu);
	return E_NOTIMPL;
}

STDMETHODIMP CWabbitemu::SetBreakpoint(IPage *pPage, WORD wAddress, VARIANT varCalcNotify)
{
	VARIANT_BOOL IsFlash;
	pPage->get_IsFlash(&IsFlash);

	int iPage;
	pPage->get_Index(&iPage);

	set_break(&m_lpCalc->mem_c, !IsFlash, iPage, wAddress);

	if (V_VT(&varCalcNotify) == VT_UNKNOWN)
	{
		if (m_lpCalc->pCalcNotify != NULL)
		{
			m_lpCalc->pCalcNotify->Release();
		}
		V_UNKNOWN(&varCalcNotify)->QueryInterface(IID_ICalcNotify, (LPVOID *) &m_lpCalc->pCalcNotify);
	}
	return S_OK;
}


STDMETHODIMP CWabbitemu::RAM(int Index, IPage **ppPage)
{
	CPage *pPage = new CComObject<CPage>();
	pPage->AddRef();
	pPage->Initialize(&m_lpCalc->mem_c, FALSE, Index);
	*ppPage = (IPage *) pPage;
	return S_OK;
}

STDMETHODIMP CWabbitemu::Flash(int Index, IPage **ppPage)
{
	CPage *pPage = new CComObject<CPage>();
	pPage->AddRef();
	pPage->Initialize(&m_lpCalc->mem_c, TRUE, Index);
	*ppPage = (IPage *) pPage;
	return S_OK;
}

STDMETHODIMP CWabbitemu::Read(WORD Address, VARIANT varByteCount, LPVARIANT lpvarResult)
{
	int nBytes = 1;
	if ((V_VT(&varByteCount) != VT_EMPTY) && (V_VT(&varByteCount) != VT_ERROR))
	{
		nBytes = V_I4(&varByteCount);
	}

	VARIANT varResult;
	VariantInit(&varResult);
	
	if (nBytes == 1)
	{
		V_VT(&varResult) = VT_UI1;
		V_UI1(&varResult) = mem_read(&m_lpCalc->mem_c, Address);
	}
	else
	{
		V_VT(&varResult) = VT_ARRAY | VT_UI1;

		SAFEARRAYBOUND sab = {0};
		sab.cElements = nBytes;
		sab.lLbound = 0;
		LPSAFEARRAY psa = SafeArrayCreate(VT_UI1, 1, &sab);

		LPBYTE lpData = NULL;
		SafeArrayAccessData(psa, (LPVOID *) &lpData);
		for (int i = 0; i < nBytes; i++)
		{
			lpData[i] = mem_read(&m_lpCalc->mem_c, Address + i);
		}
		SafeArrayUnaccessData(psa);

		V_ARRAY(&varResult) = psa;
	}
	*lpvarResult = varResult;
	return S_OK;
}

STDMETHODIMP CWabbitemu::Write(WORD Address, VARIANT varValue)
{
	if (V_VT(&varValue) & VT_ARRAY)
	{
		LONG LBound, UBound;
		SafeArrayGetLBound(V_ARRAY(&varValue), 1, &LBound);
		SafeArrayGetUBound(V_ARRAY(&varValue), 1, &UBound);

		LPBYTE lpData = NULL;
		SafeArrayAccessData(V_ARRAY(&varValue), (LPVOID *) &lpData);
		for (int i = 0; i < UBound - LBound + 1; i++)
		{
			mem_write(&m_lpCalc->mem_c, Address + i, lpData[i]);
		}
		SafeArrayUnaccessData(V_ARRAY(&varValue));
	}
	else
	{
		mem_write(&m_lpCalc->mem_c, Address, (char) V_I4(&varValue));
	}
	return S_OK;
}

STDMETHODIMP CWabbitemu::LoadFile(BSTR bstrFileName)
{
#ifdef _UNICODE
	SendFileToCalc(m_lpCalc, bstrFileName, FALSE);
#else
	char szFileName[MAX_PATH];
	WideCharToMultiByte(CP_ACP, 0, bstrFileName, -1, szFileName, sizeof(szFileName), NULL, NULL);
	SendFileToCalc(m_lpCalc, szFileName, FALSE);
#endif
	
	return S_OK;
}

STDMETHODIMP CWabbitemu::get_Apps(SAFEARRAY **ppAppList)
{
	ITypeLib *pTypeLib = NULL;
	HRESULT hr = LoadRegTypeLib(LIBID_WabbitemuLib, 1, 0, GetUserDefaultLCID(), &pTypeLib);
	if (FAILED(hr))
	{
		return hr;
	}
	ITypeInfo *pTypeInfo = NULL;
	hr = pTypeLib->GetTypeInfoOfGuid(__uuidof(TIApplication), &pTypeInfo);
	if (FAILED(hr))
	{
		return hr;
	}

	IRecordInfo *pRecordInfo;
	hr = GetRecordInfoFromTypeInfo(pTypeInfo, &pRecordInfo);
	if (FAILED(hr))
	{
		return hr;
	}
	pTypeInfo->Release();
	pTypeLib->Release();

	applist_t applist;
	state_build_applist(&m_lpCalc->cpu, &applist);

	SAFEARRAYBOUND sab = {0};
	sab.lLbound = 0;
	sab.cElements = applist.count;
	LPSAFEARRAY lpsa = SafeArrayCreateEx(VT_RECORD, 1, &sab, pRecordInfo);
	pRecordInfo->Release();

	TIApplication *pvData = NULL;
	if (SUCCEEDED(SafeArrayAccessData(lpsa, (LPVOID *) &pvData)))
	{
		for (u_int i = 0; i < sab.cElements; i++)
		{
#ifdef _UNICODE
			pvData[i].Name = SysAllocString((OLECHAR *) applist.apps[i].name);
#else
			WCHAR wszAppName[ARRAYSIZE(applist.apps[i].name)];
			MultiByteToWideChar(CP_ACP, 0, applist.apps[i].name, -1, wszAppName, ARRAYSIZE(wszAppName));
			pvData[i].Name = SysAllocString((OLECHAR *) wszAppName);
#endif
			this->Flash(applist.apps[i].page, &pvData[i].Page);
			pvData[i].PageCount = applist.apps[i].page_count;
		}

		SafeArrayUnaccessData(lpsa);
	}

	*ppAppList = lpsa;
	return S_OK;
}

STDMETHODIMP CWabbitemu::get_Symbols(SAFEARRAY **ppAppList)
{
	ITypeLib *pTypeLib = NULL;
	HRESULT hr = LoadRegTypeLib(LIBID_WabbitemuLib, 1, 0, GetUserDefaultLCID(), &pTypeLib);
	if (FAILED(hr))
	{
		return hr;
	}
	ITypeInfo *pTypeInfo = NULL;
	hr = pTypeLib->GetTypeInfoOfGuid(__uuidof(TISymbol), &pTypeInfo);
	if (FAILED(hr))
	{
		return hr;
	}

	IRecordInfo *pRecordInfo;
	hr = GetRecordInfoFromTypeInfo(pTypeInfo, &pRecordInfo);
	if (FAILED(hr))
	{
		return hr;
	}
	pTypeInfo->Release();
	pTypeLib->Release();

	symlist_t symlist;
	state_build_symlist_83P(&m_lpCalc->cpu, &symlist);

	SAFEARRAYBOUND sab = {0};
	sab.lLbound = 0;
	sab.cElements = (u_int) (symlist.last - symlist.symbols + 1);
	LPSAFEARRAY lpsa = SafeArrayCreateEx(VT_RECORD, 1, &sab, pRecordInfo);
	pRecordInfo->Release();

	TISymbol *pvData = NULL;
	if (SUCCEEDED(SafeArrayAccessData(lpsa, (LPVOID *) &pvData)))
	{
		for (u_int i = 0; i < sab.cElements; i++)
		{
			WCHAR wszSymName[256];
#ifdef _UNICODE
			if (Symbol_Name_to_String(&symlist.symbols[i], wszSymName) == NULL)
				StringCbCopy(wszSymName, sizeof(wszSymName), _T(""));
#else
			TCHAR buffer[256];
			if (Symbol_Name_to_String(&symlist.symbols[i], buffer) == NULL)
				StringCbCopy(buffer, sizeof(wszSymName), _T(""));
			MultiByteToWideChar(CP_ACP, 0, buffer, -1, wszSymName, ARRAYSIZE(wszSymName));
#endif
			pvData[i].Name = SysAllocString(wszSymName);
			pvData[i].Page = symlist.symbols[i].page;
			pvData[i].Version = symlist.symbols[i].version;
			pvData[i].Type = (SYMBOLTYPE) symlist.symbols[i].type_ID;
			pvData[i].Address = symlist.symbols[i].address;
		}

		SafeArrayUnaccessData(lpsa);
	}

	*ppAppList = lpsa;
	return S_OK;
}


static LRESULT CALLBACK  WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
		{
			SetTimer(hwnd, 1, TPF, NULL);
			return 0;
		}
	case WM_TIMER:
		{
			switch (wParam)
			{
			case 1:
				{
					static LONG difference;
					static DWORD prevTimer;

					DWORD dwTimer = GetTickCount();
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
					// Frame skip if we're too far ahead.
					} else difference += TPF;
					int i;
					for (i = 0; i < MAX_CALCS; i++) {
						if (calcs[i].active) {
							gui_draw(&calcs[i]);
						}
					}
					break;
				}
			}
			return 0;
		}
	default:
		{
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		}
	}
}

DWORD CALLBACK CWabbitemu::WabbitemuThread(LPVOID lpParam)
{
	CWabbitemu *pWabbitemu = (CWabbitemu *) lpParam;
	WNDCLASS wc = {0};

	RegisterWindowClasses();
	wc.lpszClassName = _T("WabbitemuCOMListener");
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpfnWndProc = WndProc;
	RegisterClass(&wc);

	pWabbitemu->m_hwnd = CreateWindowEx(0, _T("WabbitemuCOMListener"), _T(""), WS_CHILD, 0, 0, 0, 0, HWND_MESSAGE, 0, GetModuleHandle(NULL), NULL);

#ifdef USE_GDIPLUS
	// Initialize GDI+.
	GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
#endif

	CoInitializeEx(NULL, COINIT_MULTITHREADED);
	//SetTimer(NULL, 0, TPF, TimerProc);

	MSG Msg;
	while (GetMessage(&Msg, NULL, 0, 0))
	{
		if (Msg.message == WM_ADDFRAME)
		{
			calc_t *lpCalc = (calc_t *) Msg.lParam;
			gui_frame(lpCalc);
			HMENU hMenu = GetSystemMenu(lpCalc->hwndFrame, FALSE);
			EnableMenuItem(hMenu, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
			SetMenu(lpCalc->hwndFrame, NULL);
			RECT wr;
			GetWindowRect(lpCalc->hwndFrame, &wr);
			SendMessage(lpCalc->hwndFrame, WM_SIZING, WMSZ_BOTTOMRIGHT, (LPARAM) &wr);
			SetWindowPos(lpCalc->hwndFrame, NULL, wr.left, wr.top, wr.right - wr.left, wr.bottom - wr.top, SWP_NOZORDER);
		}
		else if (Msg.message == WM_REMOVEFRAME)
		{
			DestroyWindow(((calc_t *) Msg.lParam)->hwndFrame);
		}
		else
		{
			TranslateMessage(&Msg);
			DispatchMessage(&Msg);
		}
	}

	return 0;
}

STDMETHODIMP CWabbitemu::get_Keypad(IKeypad **ppKeypad)
{
	return m_pKeypad->QueryInterface(IID_IKeypad, (LPVOID *) ppKeypad);
}

STDMETHODIMP CWabbitemu::get_Labels(ILabelServer **ppLabelServer)
{
	return m_LabelServer.QueryInterface(IID_ILabelServer, (LPVOID *) ppLabelServer);
}