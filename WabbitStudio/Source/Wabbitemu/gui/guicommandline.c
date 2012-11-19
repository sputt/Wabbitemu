#include "stdafx.h"

#include "gui.h"

#include "guicommandline.h"
#include "calc.h"
#include "sendfileswindows.h"

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
			ZeroMemory(tmpstring, 512);
#ifdef _UNICODE
			_tcscpy(tmpstring, argv[i]);
#else
			size_t numConv;
			wcstombs_s(&numConv, tmpstring, argv[i], 512);
#endif
			char secondChar = toupper(tmpstring[1]);
			if (*tmpstring != '-' && *tmpstring != '/') {
				TCHAR *temp = (TCHAR *) malloc(_tcslen(tmpstring) + 1);
				StringCbCopy(temp, _tcslen(tmpstring) + 1, tmpstring);
				temp[_tcslen(tmpstring) + 1] = '\0';
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
			} else if (_tcslen(tmpstring) == 2) {
				if (secondChar == 'R') {
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
			} else {
				HRESULT hr = E_FAIL;
				bool fResult = _Module.ParseCommandLine(tmpstring, &hr);
				if (FAILED(hr))
				{
					OutputDebugString(_T("Failed to register\n"));
				}
			}
		}
	}
	return parsedArgs;
}

void LoadAlreadyExistingWabbit(LPARAM lParam, LPTSTR filePath, SEND_FLAG sendLoc)
{
	HWND hwnd = (HWND) lParam;
	COPYDATASTRUCT *cds = (COPYDATASTRUCT *) malloc(sizeof(COPYDATASTRUCT));
	cds->dwData = sendLoc;
	size_t strLen;
	cds->lpData = filePath;
	if (PathIsRelative(filePath)) {
		TCHAR tempPath[MAX_PATH];
		TCHAR *tempPath2 = (TCHAR *) malloc(MAX_PATH);
		_tgetcwd(tempPath, MAX_PATH);
		PathCombine(tempPath2, tempPath, filePath);
		cds->lpData = tempPath2;
	}
	StringCbLength(filePath, 512, &strLen);
	cds->cbData = strLen;
	SendMessage(hwnd, WM_COPYDATA, (WPARAM) NULL, (LPARAM) cds);
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