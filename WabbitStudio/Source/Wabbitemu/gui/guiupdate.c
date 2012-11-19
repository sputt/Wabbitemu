#include "guiupdate.h"

#include "gui.h"
#include "guidialog.h"
#include "guiwizard.h"
#include "fileutilities.h"
#include "registry.h"
#include "resource.h"

extern HINSTANCE g_hInst;

void UpdateWabbitemu() {
	TCHAR buffer[MAX_PATH];
	GetAppDataString(buffer, sizeof(buffer));
	StringCbCat(buffer, sizeof(buffer), _T("Revsoft.Autoupdater.exe"));
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
	if (!CreateProcess(NULL, argBuf, NULL, NULL, FALSE, CREATE_DEFAULT_ERROR_MODE, NULL, NULL, &si, &pi)) {
		MessageBox(NULL, _T("Unable to start the process. Try manually downloading the update from the website."), _T("Error"), MB_OK);
		ShellExecute(NULL, _T("open"), g_szWebPage, NULL, NULL, SW_SHOWNORMAL);
		return;
	}
	exit(0);
}

/*
 * Gets the version of the current file.
 * 
 * returns: FileInfo struct filled
 *			Pointer to versionData that needs to be freed when struct
 *			has been used.
 */
VS_FIXEDFILEINFO *GetFileCurrentVersion(LPBYTE *versionData) {
	VS_FIXEDFILEINFO *thisFileInfo;
	TCHAR fileName[MAX_PATH];
	DWORD dwHandle;
	UINT dwBytes;
	HRESULT error = GetModuleFileName(NULL, fileName, ARRAYSIZE(fileName));
	if (error == 0) {
		*versionData = NULL;
		return NULL;
	}
	DWORD cchVer = GetFileVersionInfoSize(fileName, &dwHandle);
	if (cchVer == 0) {
		//couldn't find the file, maybe move, deleted, or renamed
		*versionData = NULL;
		return NULL;
	}
	*versionData = (LPBYTE) malloc(cchVer);
	GetFileVersionInfo(fileName, 0, cchVer, *versionData);
	VerQueryValue(*versionData, _T("\\"), (LPVOID *) &thisFileInfo, &dwBytes);
	return thisFileInfo;
}

void GetFileCurrentVersionString(TCHAR *buf, size_t len) {
	LPBYTE versionData = NULL;
	VS_FIXEDFILEINFO *fileInfo = GetFileCurrentVersion(&versionData);
	if (versionData != NULL) {
		if (fileInfo != NULL) {
			StringCbPrintf(buf, len, _T("%d.%d.%d.%d"), HIWORD(fileInfo->dwFileVersionMS), LOWORD(fileInfo->dwFileVersionMS),
							HIWORD(fileInfo->dwFileVersionLS), LOWORD(fileInfo->dwFileVersionLS));
		}
		free(versionData);
	}
}

DWORD WINAPI CheckForUpdates(LPVOID lpParam) {	
	TCHAR fileBuffer[256];
	ZeroMemory(fileBuffer, sizeof(fileBuffer));
	HINTERNET hInternet = InternetOpen(_T("Wabbitemu"), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (hInternet == NULL) {
		return FALSE;
	}
	HINTERNET hOpenUrl = InternetOpenUrl(hInternet, g_szVersionFile, NULL, 0, INTERNET_FLAG_RELOAD, NULL);
	if (hOpenUrl == NULL) {
		return FALSE;
	}
	DWORD bytesRead;
	BOOL succeeded = InternetReadFile(hOpenUrl, fileBuffer, 256, &bytesRead);
	if (!succeeded) {
		return FALSE;
	}
	InternetCloseHandle(hInternet);
	VS_FIXEDFILEINFO newFileInfo, *thisFileInfo;
	ZeroMemory(&newFileInfo, sizeof(newFileInfo));

	_stscanf_s(fileBuffer, _T("%u.%u.%u.%u"), &newFileInfo.dwFileVersionMS, &newFileInfo.dwFileVersionLS,
					&newFileInfo.dwProductVersionMS, &newFileInfo.dwProductVersionLS);
	newFileInfo.dwFileVersionMS = MAKELONG(newFileInfo.dwFileVersionLS, newFileInfo.dwFileVersionMS);
	newFileInfo.dwFileVersionLS = MAKELONG(newFileInfo.dwProductVersionLS, newFileInfo.dwProductVersionMS);

	BOOL hasNewUpdate = TRUE;
	LPBYTE versionData = NULL;
	thisFileInfo = GetFileCurrentVersion(&versionData);
	if (!thisFileInfo) {
		return FALSE;
	}
	if ((newFileInfo.dwFileVersionMS <= thisFileInfo->dwFileVersionMS) &&
		(newFileInfo.dwFileVersionLS <= thisFileInfo->dwFileVersionLS)) {
		hasNewUpdate = FALSE;
	}
	free(versionData);
	
	if (hasNewUpdate) {
		if (MessageBox((HWND) lpParam, _T("There is a an update for Wabbitemu would you like update?"),
						_T("Update"), MB_YESNO) == IDYES) {
			UpdateWabbitemu();
		}
	}
	return hasNewUpdate;
}

BOOL IsJustUpgraded() {
	BOOL isUpgrade = TRUE;
	LPBYTE versionData = NULL;
	VS_FIXEDFILEINFO *thisFileInfo = GetFileCurrentVersion(&versionData);
	if (versionData != NULL) {
		if (thisFileInfo != NULL) {
			TCHAR *oldVersion = (TCHAR *) QueryWabbitKey("version");
			VS_FIXEDFILEINFO oldFileInfo ;
			_stscanf_s(oldVersion, _T("%u.%u.%u.%u"), &oldFileInfo.dwFileVersionMS, &oldFileInfo.dwFileVersionLS,
					&oldFileInfo.dwProductVersionMS, &oldFileInfo.dwProductVersionLS);
			oldFileInfo.dwFileVersionMS = MAKELONG(oldFileInfo.dwFileVersionLS, oldFileInfo.dwFileVersionMS);
			oldFileInfo.dwFileVersionLS = MAKELONG(oldFileInfo.dwProductVersionLS, oldFileInfo.dwProductVersionMS);
			if ((oldFileInfo.dwFileVersionMS >= thisFileInfo->dwFileVersionMS) &&
				(oldFileInfo.dwFileVersionLS >= thisFileInfo->dwFileVersionLS)) {
				isUpgrade = FALSE;
			}
		}
		free(versionData);
	}
	return isUpgrade;
}

TCHAR *GetWhatsNewText() {
#define WHATSNEWBUFFERSIZE 32768
	TCHAR *whatsNewText = (TCHAR *) malloc(sizeof(TCHAR) * WHATSNEWBUFFERSIZE);
	if (!whatsNewText) {
		return NULL;
	}
	ZeroMemory(whatsNewText, WHATSNEWBUFFERSIZE * sizeof(TCHAR));
	HINTERNET hInternet = InternetOpen(_T("Wabbitemu"), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (hInternet == NULL) {
		return NULL;
	}
	HINTERNET hOpenUrl = InternetOpenUrl(hInternet, g_szWhatsNewFile, NULL, 0, INTERNET_FLAG_RELOAD, NULL);
	if (hOpenUrl == NULL) {
		return NULL;
	}
	DWORD bytesRead;
	BOOL succeeded = InternetReadFile(hOpenUrl, whatsNewText, WHATSNEWBUFFERSIZE, &bytesRead);
	if (!succeeded) {
		return NULL;
	}
	InternetCloseHandle(hInternet);
	return whatsNewText;
}

void ShowWhatsNew(BOOL forceShow) {
	if (!IsJustUpgraded() && !forceShow) {
		return;
	}
	HWND hwndDlg = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_WHATSNEW), NULL, (DLGPROC) WhatsNewDialogProc);
	ShowWindow(hwndDlg, SW_SHOW);
}