#define UNICODE

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#pragma comment(lib,"comctl32")
#pragma comment(lib,"shlwapi")
#pragma comment(lib,"rpcrt4")

#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>

#define IOCTL_CDROM_BASE FILE_DEVICE_CD_ROM
#define IOCTL_CDROM_MEDIA_REMOVAL CTL_CODE\
(IOCTL_CDROM_BASE,0x0201,METHOD_BUFFERED,FILE_READ_ACCESS)
#define IOCTL_CDROM_GET_DRIVE_GEOMETRY CTL_CODE\
(IOCTL_CDROM_BASE,0x0013,METHOD_BUFFERED,FILE_READ_ACCESS)
#define WM_ENDTHREAD	(WM_APP+100)
#define INFOBUF_SIZE	1024
#define ID_STATIC1		101
#define ID_COMBOBOX		102
#define ID_STATIC2		103
#define ID_EDITBOX		104
#define ID_STATIC3		105
#define ID_PROGRESSBAR	106
#define ID_BUTTONSTART	107
#define ID_BUTTONCANCEL	108
#define ID_BUTTONEXIT	109

BOOL gbRun;
TCHAR szClassName[]=TEXT("DISC2ISO");
HWND hProgress,hWnd;
HHOOK g_hHook;

BOOL CreateGUID(TCHAR *szGUID)
{
	GUID m_guid = GUID_NULL;
	HRESULT hr = UuidCreate(&m_guid);
	if (HRESULT_CODE(hr) != RPC_S_OK){ return FALSE; }
	if (m_guid == GUID_NULL){ return FALSE; }
	wsprintf(szGUID, TEXT("{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}"),
		m_guid.Data1, m_guid.Data2, m_guid.Data3,
		m_guid.Data4[0], m_guid.Data4[1], m_guid.Data4[2], m_guid.Data4[3],
		m_guid.Data4[4], m_guid.Data4[5], m_guid.Data4[6], m_guid.Data4[7]);
	return TRUE;
}

BOOL CreateTempDirectory(LPTSTR pszDir)
{
	TCHAR szGUID[39];
	if (GetTempPath(MAX_PATH, pszDir) == 0)return FALSE;
	if (CreateGUID(szGUID) == 0)return FALSE;
	if (PathAppend(pszDir, szGUID) == 0)return FALSE;
	if (CreateDirectory(pszDir, 0) == 0)return FALSE;
	return TRUE;
}

LRESULT CALLBACK CBTProc(int nCode,WPARAM wParam,LPARAM lParam)
{
	if(nCode==HCBT_ACTIVATE)
	{
        HWND hMes=(HWND)wParam;
        RECT m,w;
        GetWindowRect(hMes,&m);
        GetWindowRect(hWnd,&w);
        SetWindowPos(
			hMes,
			hWnd,
			(w.right+w.left-m.right+m.left)/2,
			(w.bottom+w.top-m.bottom+m.top)/2,
			0,
			0,
			SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
        UnhookWindowsHookEx(g_hHook);
    }
    return 0;
}

DWORD WINAPI ThreadFunc(LPVOID)
{
	ULONGLONG TotalSize=0;
	TCHAR szText[1024];
	TCHAR szFileName[1024];
	TCHAR szVolumeName[1024];
	HANDLE hCD;
	DWORD dwNotUsed;
	GetDlgItemText(hWnd, ID_EDITBOX, szFileName, 1024);
	TCHAR szTempDirectoryPath[MAX_PATH];
	if (CreateTempDirectory(szTempDirectoryPath))
	{
		PathAppend(szTempDirectoryPath, szFileName);
		HANDLE hFile = CreateFile(
			szTempDirectoryPath,
			GENERIC_WRITE,
			0,
			0,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			0);
		if (hFile != INVALID_HANDLE_VALUE)
		{
			GetDlgItemText(hWnd, ID_COMBOBOX, szText, 1024);
			LPTSTR ptr = szText;
			while (*ptr)
			{
				if (*ptr == TEXT(' '))
				{
					*ptr = TEXT('\0'); break;
				}
				ptr++;
			}
			ULARGE_INTEGER dwlTotalNumberOfBytes;
			if (GetDiskFreeSpaceEx(
				szText,
				0,
				&dwlTotalNumberOfBytes,
				0)
				)
				TotalSize = dwlTotalNumberOfBytes.QuadPart;
			PathStripToRoot(szText);
			if (szText[lstrlen(szText) - 1] == TEXT('\\'))
			{
				szText[lstrlen(szText) - 1] = 0;
			}
			lstrcpy(szVolumeName, TEXT("\\\\.\\"));
			lstrcat(szVolumeName, szText);
			hCD = CreateFile(
				szVolumeName,
				GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				0,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				0);
			if (hCD != INVALID_HANDLE_VALUE)
			{
				DISK_GEOMETRY dgCDROM;
				PREVENT_MEDIA_REMOVAL pmrLockCDROM;
				pmrLockCDROM.PreventMediaRemoval = TRUE;
				DeviceIoControl(
					hCD,
					IOCTL_CDROM_MEDIA_REMOVAL,
					&pmrLockCDROM,
					sizeof(pmrLockCDROM),
					0,
					0,
					&dwNotUsed,
					0);
				if (DeviceIoControl(
					hCD,
					IOCTL_CDROM_GET_DRIVE_GEOMETRY,
					0,
					0,
					&dgCDROM,
					sizeof(dgCDROM),
					&dwNotUsed,
					0))
				{
					DWORD percent = 0;
					LPBYTE lpSector;
					DWORD dwSize = dgCDROM.BytesPerSector;
					INT64 lSectors = TotalSize / dwSize;
					lpSector = (LPBYTE)VirtualAlloc(
						0,
						dwSize,
						MEM_COMMIT | MEM_RESERVE,
						PAGE_READWRITE);
					for (INT64 i = 0; i < lSectors; i++)
					{
						DWORD temp = (DWORD)(256 * i / lSectors);
						if (percent != temp)
						{
							percent = temp;
							SendMessage(hProgress, PBM_STEPIT, 0, 0);
						}
						if (gbRun == FALSE)break;
						LARGE_INTEGER liStart;
						liStart.QuadPart = dgCDROM.BytesPerSector*i;
						if (0 == SetFilePointerEx(
							hCD,
							liStart,
							0, FILE_BEGIN
							))break;
						if (0 == ReadFile(
							hCD,
							lpSector,
							dwSize,
							&dwNotUsed,
							0))break;
						WriteFile(
							hFile,
							lpSector,
							dwSize,
							&dwNotUsed,
							0);
					}
					VirtualFree(lpSector, 0, MEM_RELEASE);
				}
				pmrLockCDROM.PreventMediaRemoval = FALSE;
				DeviceIoControl(
					hCD,
					IOCTL_CDROM_MEDIA_REMOVAL,
					&pmrLockCDROM,
					sizeof(pmrLockCDROM),
					0,
					0,
					&dwNotUsed,
					0);
				CloseHandle(hCD);
			}
			CloseHandle(hFile);
			OPENFILENAME ofn;
			ZeroMemory(&ofn, sizeof(ofn));
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.hwndOwner = hWnd;
			ofn.lpstrFilter = TEXT("ISO(*.iso)\0*.txt\0すべてのファイル(*.*)\0*.*\0\0");
			ofn.lpstrFile = szFileName;
			ofn.nMaxFile = sizeof(szFileName);
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_OVERWRITEPROMPT;
			if (GetSaveFileName(&ofn))
			{
				MoveFile(szTempDirectoryPath, szFileName);
				g_hHook = SetWindowsHookEx(
					WH_CBT,
					CBTProc,
					0,
					GetCurrentThreadId());
				MessageBox(
					hWnd,
					TEXT("完了しました。"),
					TEXT("確認"),
					MB_ICONINFORMATION);
			}
			else
			{
				DeleteFile(szTempDirectoryPath);
			}
			PathRemoveFileSpec(szTempDirectoryPath);
			RemoveDirectory(szTempDirectoryPath);
		}
	}
	PostMessage(hWnd,WM_ENDTHREAD,0,0);
	ExitThread(0);	
}

LRESULT CALLBACK WndProc(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	static HFONT hFont;
	static HANDLE hThread=0;
	switch(msg)
	{
		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
				case ID_EDITBOX:
					if(HIWORD(wParam)==EN_SETFOCUS)PostMessage(
						GetDlgItem(hWnd,LOWORD(wParam)),
						EM_SETSEL,
						(WPARAM)0,
						(LPARAM)-1);
					break;
				case IDOK:
				case ID_BUTTONSTART:
					if(GetWindowTextLength(
						GetDlgItem(hWnd,ID_EDITBOX))==0)
					{
						g_hHook=SetWindowsHookEx(
							WH_CBT,
							CBTProc,
							0,
							GetCurrentThreadId());
						MessageBox(
							hWnd,
							TEXT("出力ファイル名を入力してください。"),
							TEXT("確認"),
							MB_ICONINFORMATION);
						SetFocus(GetDlgItem(hWnd,ID_EDITBOX));
						break;
					}
					EnableWindow(GetDlgItem(hWnd,ID_COMBOBOX),FALSE);
					EnableWindow(GetDlgItem(hWnd,ID_EDITBOX),FALSE);
					EnableWindow(GetDlgItem(hWnd,ID_BUTTONSTART),FALSE);
					gbRun=TRUE;
					hThread=CreateThread(0,0,ThreadFunc,0,0,0);
					EnableWindow(GetDlgItem(hWnd,ID_BUTTONCANCEL),TRUE);
					break;
				case ID_BUTTONCANCEL:
					if(gbRun==TRUE)
					{
						g_hHook=SetWindowsHookEx(
							WH_CBT,
							CBTProc,
							0,
							GetCurrentThreadId());
						if(IDYES==MessageBox(
							hWnd,
							TEXT("中断しますか？"),
							TEXT("確認"),
							MB_YESNO|MB_ICONINFORMATION))
							gbRun=FALSE;
					}
					break;
				case ID_COMBOBOX:
					if(HIWORD(wParam)==CBN_SELCHANGE)
					{
						TCHAR szText[1024];
						GetDlgItemText(hWnd,ID_COMBOBOX,szText,1024);
						LPTSTR ptr=szText;
						while(*ptr)
						{
							if(*ptr==TEXT(' '))
							{
								*ptr=TEXT('\0');
								break;
							}
							ptr++;
						}
						TCHAR szVolumeName[1024];
						if(GetVolumeInformation(
							szText,
							szVolumeName,
							1024,0,0,0,0,0))
						{
							lstrcat(szVolumeName,TEXT(".ISO"));
							SetDlgItemText(hWnd,ID_EDITBOX,szVolumeName);
						}else
							SetDlgItemText(hWnd,ID_EDITBOX,0);
					}
					break;
				case IDCANCEL:
				case ID_BUTTONEXIT:
					PostMessage(hWnd,WM_CLOSE,0,0);
			}
			break;
		case WM_CREATE:
			InitCommonControls();
			hFont = CreateFont(22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, TEXT("メイリオ"));
			{
				const HWND hStatic1 = CreateWindow(
					TEXT("STATIC"),
					TEXT("ドライブ選択(&D):"),
					WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE,
					4, 4, 128 + 64, 32,
					hWnd,
					(HMENU)ID_STATIC1,
					((LPCREATESTRUCT)lParam)->hInstance,
					0);
				const HWND hCombo = CreateWindow(
					TEXT("COMBOBOX"),
					0,
					WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST,
					128 + 64 + 8, 4, 256, 128,
					hWnd,
					(HMENU)ID_COMBOBOX,
					((LPCREATESTRUCT)lParam)->hInstance,
					0);
				const HWND hStatic2 = CreateWindow(
					TEXT("STATIC"),
					TEXT("出力ファイル名(&F):"),
					WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE,
					4, 32 + 8, 128 + 64, 32,
					hWnd,
					(HMENU)ID_STATIC2,
					((LPCREATESTRUCT)lParam)->hInstance,
					0);
				const HWND hEdit = CreateWindowEx(
					WS_EX_CLIENTEDGE,
					TEXT("EDIT"),
					0,
					WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
					128 + 64 + 8, 32 + 8, 256, 32,
					hWnd,
					(HMENU)ID_EDITBOX,
					((LPCREATESTRUCT)(lParam))->hInstance,
					0);
				const HWND hStatic3 = CreateWindow(
					TEXT("STATIC"),
					TEXT("進捗:"),
					WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE,
					4, 32 + 32 + 12, 128 + 64, 32,
					hWnd,
					(HMENU)ID_STATIC3,
					((LPCREATESTRUCT)lParam)->hInstance,
					0);
				hProgress = CreateWindow(
					PROGRESS_CLASS,
					0,
					WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
					128 + 64 + 8, 32 + 32 + 12, 256, 32,
					hWnd,
					(HMENU)ID_PROGRESSBAR,
					((LPCREATESTRUCT)(lParam))->hInstance,
					0);
				const HWND hButton1 = CreateWindow(
					TEXT("BUTTON"),
					TEXT("開始(&S)"),
					WS_CHILD | WS_VISIBLE | WS_TABSTOP,
					32, 32 + 32 + 32 + 16, 128, 32,
					hWnd,
					(HMENU)ID_BUTTONSTART,
					((LPCREATESTRUCT)(lParam))->hInstance,
					0);
				const HWND hButton2 = CreateWindow(
					TEXT("BUTTON"),
					TEXT("中断(&C)"),
					WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_DISABLED,
					128 + 32 + 16, 32 + 32 + 32 + 16, 128, 32,
					hWnd,
					(HMENU)ID_BUTTONCANCEL,
					((LPCREATESTRUCT)(lParam))->hInstance,
					0);
				const HWND hButton3 = CreateWindow(
					TEXT("BUTTON"),
					TEXT("閉じる(&X)"),
					WS_CHILD | WS_VISIBLE | WS_TABSTOP,
					256 + 64, 32 + 32 + 32 + 16, 128, 32,
					hWnd,
					(HMENU)ID_BUTTONEXIT,
					((LPCREATESTRUCT)(lParam))->hInstance,
					0);
				SendMessage(hStatic1, WM_SETFONT, (WPARAM)hFont, 0);
				SendMessage(hCombo, WM_SETFONT, (WPARAM)hFont, 0);
				SendMessage(hStatic2, WM_SETFONT, (WPARAM)hFont, 0);
				SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, 0);
				SendMessage(hStatic3, WM_SETFONT, (WPARAM)hFont, 0);
				SendMessage(hProgress, WM_SETFONT, (WPARAM)hFont, 0);
				SendMessage(hButton1, WM_SETFONT, (WPARAM)hFont, 0);
				SendMessage(hButton2, WM_SETFONT, (WPARAM)hFont, 0);
				SendMessage(hButton3, WM_SETFONT, (WPARAM)hFont, 0);
			}
			SendMessage(hProgress,PBM_SETRANGE,0 ,MAKELPARAM(0,256));
			SendMessage(hProgress,PBM_SETSTEP,1,0);
		case WM_DEVICECHANGE:
			if(!gbRun)
			{
				INT i=SendMessage(
					GetDlgItem(hWnd,ID_COMBOBOX),
					CB_GETCURSEL,0,0);if(i==-1)i=0;
				while(
					SendMessage(
					GetDlgItem(hWnd,ID_COMBOBOX),
					CB_GETCOUNT,0,0)!=0)
					SendMessage(
					GetDlgItem(hWnd,ID_COMBOBOX),
					CB_DELETESTRING,0,0);
				INT len=GetLogicalDriveStrings(0,0);
				LPTSTR drives=(LPTSTR)GlobalAlloc(GPTR,sizeof(TCHAR)*(len+1));
				GetLogicalDriveStrings(len,drives);
				LPCTSTR ptr=drives;
				while(*ptr)
				{
					if(DRIVE_CDROM==GetDriveType(ptr))
					{
						TCHAR szVolumeName[1024];
						TCHAR szTemp[1024];
						if(GetVolumeInformation(
							ptr,szVolumeName,1024,0,0,0,0,0))
							wsprintf(
							szTemp,TEXT("%s %s"),ptr,szVolumeName);
						else
							wsprintf(szTemp,TEXT("%s"),ptr);
						SendMessage(
							GetDlgItem(hWnd,ID_COMBOBOX),
							CB_ADDSTRING,0,(LPARAM)szTemp);
					}
					while(*ptr)ptr++;ptr++;
				}
				GlobalFree(drives);
				SendMessage(
					GetDlgItem(hWnd,ID_COMBOBOX),
					CB_SETCURSEL,i,0);
				SendMessage(
					hWnd,
					WM_COMMAND,
					MAKEWPARAM(ID_COMBOBOX,CBN_SELCHANGE),
					(LPARAM)GetDlgItem(hWnd,ID_COMBOBOX));
			}
			break;
		case WM_CLOSE:
			if(gbRun==TRUE)
			{
				g_hHook=SetWindowsHookEx(
					WH_CBT,CBTProc,0,GetCurrentThreadId());
				if(IDYES!=MessageBox(
					hWnd,TEXT("中断しますか？"),TEXT("確認"),
					MB_YESNO|MB_ICONINFORMATION))break;
			}
			DestroyWindow(hWnd);
			break;
		case WM_ENDTHREAD:
			if(gbRun)
			{
				SendMessage(hProgress,PBM_SETPOS,256,0);
				gbRun=FALSE;
			}
			SendMessage(hProgress,PBM_SETPOS,0,0);
			EnableWindow(GetDlgItem(hWnd,ID_EDITBOX),TRUE);
			EnableWindow(GetDlgItem(hWnd,ID_COMBOBOX),TRUE);
			EnableWindow(GetDlgItem(hWnd,ID_BUTTONSTART),TRUE);
			EnableWindow(GetDlgItem(hWnd,ID_BUTTONCANCEL),FALSE);
			break;
		case WM_DESTROY:
			gbRun=FALSE;
			while(WaitForSingleObject(hThread,0)==WAIT_TIMEOUT)Sleep(1);
			DeleteObject(hFont);
			PostQuitMessage(0);
			break;
		default:
			return(DefDlgProc(hWnd,msg,wParam,lParam));
	}
	return(0L);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPreInst, LPSTR pCmdLine, int nCmdShow)
{
	MSG msg;
	RECT rect;
	WNDCLASS wndclass={
		0,
		WndProc,
		0,
		DLGWINDOWEXTRA,
		hInstance,
		0,
		LoadCursor(0,IDC_ARROW),
		0,
		0,
		szClassName
	};
	RegisterClass(&wndclass);
	SetRect(&rect,0,0,256+64+128+32,32*4+4*5);
	AdjustWindowRect(&rect,WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN,0);
	hWnd=CreateWindow(
		szClassName,
		szClassName,
		WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rect.right-rect.left,
		rect.bottom-rect.top,
		0,
		0,
		hInstance,
		0);
	ShowWindow(hWnd,SW_SHOWNORMAL);
	UpdateWindow(hWnd);
	while(GetMessage(&msg,0,0,0))
	{
		if(!IsDialogMessage(hWnd,&msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return msg.wParam;
}
