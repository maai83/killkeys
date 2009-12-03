/*
	KillKeys - Disable keys from working
	Copyright (C) 2008  Stefan Sundin (recover89@gmail.com)
	
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.
	
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	
	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <shlwapi.h>

//Messages
#define WM_ICONTRAY            WM_USER+1
#define WM_ADDTRAY             WM_USER+2 //This value has to remain constant through versions
#define SWM_TOGGLE             WM_APP+1
#define SWM_HIDE               WM_APP+2
#define SWM_AUTOSTART_ON       WM_APP+3
#define SWM_AUTOSTART_OFF      WM_APP+4
#define SWM_AUTOSTART_HIDE_ON  WM_APP+5
#define SWM_AUTOSTART_HIDE_OFF WM_APP+6
#define SWM_ABOUT              WM_APP+7
#define SWM_EXIT               WM_APP+8

//Stuff
LRESULT CALLBACK MyWndProc(HWND, UINT, WPARAM, LPARAM);

static HICON icon[2];
static NOTIFYICONDATA traydata;
static UINT WM_TASKBARCREATED;
static int tray_added=0;
static int hide=0;

static HINSTANCE hinstDLL;
static HHOOK keyhook;
static int hook_installed=0;
static int *keys=NULL;
static int numkeys=0;
static int *keys_fullscreen=NULL;
static int numkeys_fullscreen=0;

static char txt[1000];

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInstance, LPSTR szCmdLine, int iCmdShow) {
	//Look for previous instance
	HWND previnst;
	if ((previnst=FindWindow("KillKeys",NULL)) != NULL) {
		SendMessage(previnst,WM_ADDTRAY,0,0);
		return 0;
	}

	//Change working directory
	char path[MAX_PATH];
	if (GetModuleFileName(NULL, path, sizeof(path)) == 0) {
		sprintf(txt,"GetModuleFileName() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "KillKeys Warning", MB_ICONWARNING|MB_OK);
	}
	PathRemoveFileSpec(path);
	if (SetCurrentDirectory(path) == 0) {
		sprintf(txt,"SetCurrentDirectory() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "KillKeys Warning", MB_ICONWARNING|MB_OK);
	}

	//Check command line
	if (!strcmp(szCmdLine,"-hide")) {
		hide=1;
	}

	//Create window class
	WNDCLASS wnd;
	wnd.style=CS_HREDRAW | CS_VREDRAW;
	wnd.lpfnWndProc=MyWndProc;
	wnd.cbClsExtra=0;
	wnd.cbWndExtra=0;
	wnd.hInstance=hInst;
	wnd.hIcon=LoadImage(NULL, IDI_APPLICATION, IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
	wnd.hCursor=LoadImage(NULL, IDC_ARROW, IMAGE_CURSOR, 0, 0, LR_DEFAULTCOLOR);
	wnd.hbrBackground=(HBRUSH)(COLOR_BACKGROUND+1);
	wnd.lpszMenuName=NULL;
	wnd.lpszClassName="KillKeys";
	
	//Register class
	if (RegisterClass(&wnd) == 0) {
		sprintf(txt,"RegisterClass() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "KillKeys Error", MB_ICONERROR|MB_OK);
		return 1;
	}
	
	//Create window
	HWND hwnd=CreateWindow(wnd.lpszClassName, "KillKeys", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInst, NULL);
	
	//Register TaskbarCreated so we can re-add the tray icon if explorer.exe crashes
	if ((WM_TASKBARCREATED=RegisterWindowMessage("TaskbarCreated")) == 0) {
		sprintf(txt,"RegisterWindowMessage() failed (error code: %d) in file %s, line %d.\nThis means the tray icon won't be added if (or should I say when) explorer.exe crashes.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "KillKeys Warning", MB_ICONWARNING|MB_OK);
	}
	
	//Load tray icons
	if ((icon[0] = LoadImage(hInst, "tray-disabled", IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR)) == NULL) {
		sprintf(txt,"LoadImage() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "KillKeys Error", MB_ICONERROR|MB_OK);
		PostQuitMessage(1);
	}
	if ((icon[1] = LoadImage(hInst, "tray-enabled", IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR)) == NULL) {
		sprintf(txt,"LoadImage() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "KillKeys Error", MB_ICONERROR|MB_OK);
		PostQuitMessage(1);
	}
	
	//Create icondata
	traydata.cbSize=sizeof(NOTIFYICONDATA);
	traydata.uID=0;
	traydata.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;
	traydata.hWnd=hwnd;
	traydata.uCallbackMessage=WM_ICONTRAY;
	
	//Add tray icon
	UpdateTray();
	
	//Install hook
	InstallHook();
	
	//Add tray if hook failed, even though -hide was supplied
	if (!hook_installed && hide) {
		hide=0;
		UpdateTray();
	}
	
	//Message loop
	MSG msg;
	while(GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.wParam;
}

void ShowContextMenu(HWND hwnd) {
	POINT pt;
	GetCursorPos(&pt);
	HMENU hMenu, hAutostartMenu;
	if ((hMenu = CreatePopupMenu()) == NULL) {
		sprintf(txt,"CreatePopupMenu() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "KillKeys Warning", MB_ICONWARNING|MB_OK);
	}
	
	//Toggle
	InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_TOGGLE, (hook_installed?"Disable":"Enable"));
	
	//Hide
	InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_HIDE, "Hide tray");
	
	//Autostart
	//Check registry
	int autostart_enabled=0, autostart_hide=0;
	//Open key
	HKEY key;
	if (RegOpenKeyEx(HKEY_CURRENT_USER,"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_QUERY_VALUE,&key) != ERROR_SUCCESS) {
		sprintf(txt,"RegOpenKeyEx() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "KillKeys Warning", MB_ICONWARNING|MB_OK);
	}
	//Read value
	char autostart_value[MAX_PATH+10];
	DWORD len=sizeof(autostart_value);
	DWORD res=RegQueryValueEx(key,"KillKeys",NULL,NULL,(LPBYTE)autostart_value,&len);
	if (res != ERROR_FILE_NOT_FOUND && res != ERROR_SUCCESS) {
		sprintf(txt,"RegQueryValueEx() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "KillKeys Warning", MB_ICONWARNING|MB_OK);
	}
	//Close key
	if (RegCloseKey(key) != ERROR_SUCCESS) {
		sprintf(txt,"RegCloseKey() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "KillKeys Warning", MB_ICONWARNING|MB_OK);
	}
	//Get path
	char path[MAX_PATH];
	if (GetModuleFileName(NULL,path,sizeof(path)) == 0) {
		sprintf(txt,"GetModuleFileName() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "KillKeys Warning", MB_ICONWARNING|MB_OK);
	}
	//Compare
	char pathcmp[MAX_PATH+10];
	sprintf(pathcmp,"\"%s\"",path);
	if (!strcmp(pathcmp,autostart_value)) {
		autostart_enabled=1;
	}
	sprintf(pathcmp,"\"%s\" -hide",path);
	if (!strcmp(pathcmp,autostart_value)) {
		autostart_enabled=1;
		autostart_hide=1;
	}
	
	if ((hAutostartMenu = CreatePopupMenu()) == NULL) {
		sprintf(txt,"CreatePopupMenu() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "KillKeys Warning", MB_ICONWARNING|MB_OK);
	}
	InsertMenu(hAutostartMenu, -1, MF_BYPOSITION|(autostart_enabled?MF_CHECKED:0), (autostart_enabled?SWM_AUTOSTART_OFF:SWM_AUTOSTART_ON), "Autostart");
	InsertMenu(hAutostartMenu, -1, MF_BYPOSITION|(autostart_hide?MF_CHECKED:0), (autostart_hide?SWM_AUTOSTART_HIDE_OFF:SWM_AUTOSTART_HIDE_ON), "Hide tray");
	InsertMenu(hMenu, -1, MF_BYPOSITION|MF_POPUP, (UINT)hAutostartMenu, "Autostart");
	InsertMenu(hMenu, -1, MF_BYPOSITION|MF_SEPARATOR, 0, NULL);
	
	//About
	InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_ABOUT, "About");
	
	//Exit
	InsertMenu(hMenu, -1, MF_BYPOSITION, SWM_EXIT, "Exit");

	//Must set window to the foreground, or else the menu won't disappear when clicking outside it
	SetForegroundWindow(hwnd);

	TrackPopupMenu(hMenu, TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, NULL );
	DestroyMenu(hMenu);
}

int UpdateTray() {
	strncpy(traydata.szTip,(hook_installed?"KillKeys (enabled)":"KillKeys (disabled)"),sizeof(traydata.szTip));
	traydata.hIcon=icon[hook_installed];
	
	//Only add or modify if not hidden
	if (!hide) {
		if (Shell_NotifyIcon((tray_added?NIM_MODIFY:NIM_ADD),&traydata) == FALSE) {
			sprintf(txt,"Failed to add tray icon.\n\nShell_NotifyIcon() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
			MessageBox(NULL, txt, "KillKeys Warning", MB_ICONWARNING|MB_OK);
			return 1;
		}
		
		//Success
		tray_added=1;
	}
}

int RemoveTray() {
	if (!tray_added) {
		//Tray not added
		return 1;
	}
	
	if (Shell_NotifyIcon(NIM_DELETE,&traydata) == FALSE) {
		sprintf(txt,"Failed to remove tray icon.\n\nShell_NotifyIcon() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "KillKeys Warning", MB_ICONWARNING|MB_OK);
		return 1;
	}
	
	//Success
	tray_added=0;
}

int InstallHook() {
	if (hook_installed) {
		//Hook already installed
		return 1;
	}
	
	//Read config.txt to buffer
	FILE *config;
	if ((config=fopen("config.txt","rb")) == NULL) {
		sprintf(txt,"fopen() failed in file %s, line %d.",__FILE__,__LINE__);
		MessageBox(NULL, txt, "KillKeys Warning", MB_ICONWARNING|MB_OK);
		return 1;
	}
	fseek(config,0,SEEK_END);
	int length=ftell(config);
	fseek(config,0,SEEK_SET);
	char *buffer=malloc(length+1);
	fread(buffer,1,length,config);
	buffer[length]='\0';
	if (fclose(config) == EOF) {
		sprintf(txt,"fclose() failed in file %s, line %d.",__FILE__,__LINE__);
		fclose(config);
		MessageBox(NULL, txt, "KillKeys Warning", MB_ICONWARNING|MB_OK);
		return 1;
	}
	
	//Parse buffer
	numkeys=0;
	numkeys_fullscreen=0;
	int keys_alloc=0;
	//Loop buffer
	int **add_keys=&keys; //We need a pointer to a pointer to be able to realloc without having to know if add_keys points to keys or keys_fullscreen
	int *add_numkeys=&numkeys;
	int pos=0;
	while (buffer[pos] != '\0') {
		if (buffer[pos] == '\r') {
			//Ignore \r
			pos++;
		}
		else if (buffer[pos] == '\n') {
			//Switch to keys_fullscreen or stop parsing
			if (*add_keys == keys) {
				add_keys=&keys_fullscreen;
				add_numkeys=&numkeys_fullscreen;
				keys_alloc=0;
			}
			else {
				break;
			}
			pos++;
		}
		else if (buffer[pos] == ' ') {
			//Ignore space
			pos++;
		}
		else {
			//Parse key
			int temp;
			int numbytesread;
			if (sscanf(buffer+pos,"%02X%n",&temp,&numbytesread) != EOF) {
				//Make sure we have enough space
				if (*add_numkeys == keys_alloc) {
					keys_alloc+=100;
					if ((*add_keys=realloc(*add_keys,keys_alloc*sizeof(int))) == NULL) {
						sprintf(txt,"realloc() failed in file %s, line %d.",__FILE__,__LINE__);
						MessageBox(NULL, txt, "KillKeys Warning", MB_ICONWARNING|MB_OK);
						return 1;
					}
				}
				//Store key
				(*add_keys)[(*add_numkeys)++]=temp;
				pos+=numbytesread;
			}
		}
	}
	
	//Free buffer
	free(buffer);
	
	//Load dll
	if ((hinstDLL=LoadLibrary((LPCTSTR)"keyhook.dll")) == NULL) {
		sprintf(txt,"Failed to load keyhook.dll.\nThis probably means that the file is missing.\nYou can try to download KillKeys again from the website.\n\nError message:\nLoadLibrary() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "KillKeys Warning", MB_ICONWARNING|MB_OK);
		return 1;
	}
	
	//Get address to settings function
	typedef void (*pfunc)();
	pfunc Settings;
	if ((Settings=(pfunc)GetProcAddress(hinstDLL,"Settings")) == NULL) {
		sprintf(txt,"GetProcAddress() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "KillKeys Warning", MB_ICONWARNING|MB_OK);
		return 1;
	}
	
	//Give keyhook settings
	Settings(keys, numkeys, keys_fullscreen, numkeys_fullscreen);
	
	//Get address to keyboard hook (beware name mangling)
	HOOKPROC procaddr;
	if ((procaddr=(HOOKPROC)GetProcAddress(hinstDLL,"KeyboardProc@12")) == NULL) {
		sprintf(txt,"GetProcAddress() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "KillKeys Warning", MB_ICONWARNING|MB_OK);
		return 1;
	}
	
	//Set up the keyboard hook
	if ((keyhook=SetWindowsHookEx(WH_KEYBOARD_LL,procaddr,hinstDLL,0)) == NULL) {
		sprintf(txt,"SetWindowsHookEx() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "KillKeys Warning", MB_ICONWARNING|MB_OK);
		return 1;
	}
	
	//Success
	hook_installed=1;
	UpdateTray();
	return 0;
}

int RemoveHook() {
	if (!hook_installed) {
		//Hook not installed
		return 1;
	}
	
	//Remove keyboard hook
	if (UnhookWindowsHookEx(keyhook) == 0) {
		sprintf(txt,"UnhookWindowsHookEx() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "KillKeys Warning", MB_ICONWARNING|MB_OK);
		return 1;
	}
	
	//Unload dll
	if (FreeLibrary(hinstDLL) == 0) {
		sprintf(txt,"FreeLibrary() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "KillKeys Warning", MB_ICONWARNING|MB_OK);
		return 1;
	}
	
	//Free keys
	numkeys=0;
	numkeys_fullscreen=0;
	free(keys);
	free(keys_fullscreen);
	keys=NULL;
	keys_fullscreen=NULL;
	
	//Success
	hook_installed=0;
	UpdateTray();
	return 0;
}

void ToggleHook() {
	if (hook_installed) {
		RemoveHook();
	}
	else {
		InstallHook();
	}
}

void SetAutostart(int on, int hide) {
	//Open key
	HKEY key;
	if (RegOpenKeyEx(HKEY_CURRENT_USER,"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_SET_VALUE,&key) != ERROR_SUCCESS) {
		sprintf(txt,"RegOpenKeyEx() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "KillKeys Warning", MB_ICONWARNING|MB_OK);
		return;
	}
	if (on) {
		//Get path
		char path[MAX_PATH];
		if (GetModuleFileName(NULL,path,sizeof(path)) == 0) {
			sprintf(txt,"GetModuleFileName() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
			MessageBox(NULL, txt, "KillKeys Warning", MB_ICONWARNING|MB_OK);
			return;
		}
		//Add
		char value[MAX_PATH+10];
		sprintf(value,(hide?"\"%s\" -hide":"\"%s\""),path);
		if (RegSetValueEx(key,"KillKeys",0,REG_SZ,value,strlen(value)+1) != ERROR_SUCCESS) {
			sprintf(txt,"RegSetValueEx() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
			MessageBox(NULL, txt, "KillKeys Warning", MB_ICONWARNING|MB_OK);
			return;
		}
	}
	else {
		//Remove
		if (RegDeleteValue(key,"KillKeys") != ERROR_SUCCESS) {
			sprintf(txt,"RegDeleteValue() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
			MessageBox(NULL, txt, "KillKeys Warning", MB_ICONWARNING|MB_OK);
			return;
		}
	}
	//Close key
	if (RegCloseKey(key) != ERROR_SUCCESS) {
		sprintf(txt,"RegCloseKey() failed (error code: %d) in file %s, line %d.",GetLastError(),__FILE__,__LINE__);
		MessageBox(NULL, txt, "KillKeys Warning", MB_ICONWARNING|MB_OK);
		return;
	}
}

LRESULT CALLBACK MyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_COMMAND) {
		int wmId=LOWORD(wParam), wmEvent=HIWORD(wParam);
		if (wmId == SWM_TOGGLE) {
			ToggleHook();
		}
		else if (wmId == SWM_HIDE) {
			hide=1;
			RemoveTray();
		}
		else if (wmId == SWM_AUTOSTART_ON) {
			SetAutostart(1,0);
		}
		else if (wmId == SWM_AUTOSTART_OFF) {
			SetAutostart(0,0);
		}
		else if (wmId == SWM_AUTOSTART_HIDE_ON) {
			SetAutostart(1,1);
		}
		else if (wmId == SWM_AUTOSTART_HIDE_OFF) {
			SetAutostart(1,0);
		}
		else if (wmId == SWM_ABOUT) {
			sprintf(txt, "KillKeys - 0.3\n\
http://killkeys.googlecode.com/\n\
recover89@gmail.com\n\
\n\
When enabled, keys specified in config.txt will be disabled.\n\
By default, both windows keys and the menu key are disabled.");
			if (hook_installed) {
				sprintf(txt, "%s\nKeys disabled when not in fullscreen:",txt);
				int i;
				for (i=0; i < numkeys; i++) {
					sprintf(txt, "%s %02X", txt, keys[i]);
				}
				sprintf(txt, "%s\nKeys disabled when in fullscreen:", txt);
				for (i=0; i < numkeys_fullscreen; i++) {
					sprintf(txt, "%s %02X", txt, keys_fullscreen[i]);
				}
			}
			else {
				sprintf(txt, "%s\nEnable KillKeys to see which keys will be blocked.",txt);
			}
			sprintf(txt, "%s\n\
\nInstructions on how to edit the disabled keys in info.txt\n\
\n\
You can use -hide as a parameter to hide the tray icon.\n\
\n\
Send feedback to recover89@gmail.com",txt);
			MessageBox(NULL, txt, "About KillKeys", MB_ICONINFORMATION|MB_OK);
		}
		else if (wmId == SWM_EXIT) {
			DestroyWindow(hwnd);
		}
	}
	else if (msg == WM_ICONTRAY) {
		if (lParam == WM_LBUTTONDOWN) {
			ToggleHook();
		}
		else if (lParam == WM_RBUTTONDOWN) {
			ShowContextMenu(hwnd);
		}
	}
	else if (msg == WM_TASKBARCREATED) {
		tray_added=0;
		UpdateTray();
	}
	else if (msg == WM_DESTROY) {
		if (hook_installed) {
			RemoveHook();
		}
		if (tray_added) {
			RemoveTray();
		}
		PostQuitMessage(0);
		return 0;
	}
	else if (msg == WM_ADDTRAY) {
		hide=0;
		UpdateTray();
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}