#undef UNICODE
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>
#include <cstdio>
#include <sstream>
#include "mhook-lib\mhook.h"

#define SBAPI __thiscall
#define SML __fastcall


// Hooks
typedef void(SBAPI *pToggleDebug)(void* obj);
typedef void(SBAPI *pBack)(void* obj);
typedef void(SBAPI *pInvBar)(void* obj);
typedef void(SBAPI *pZoom)(void* obj);
typedef void(SBAPI *pInventory)(void* obj);
typedef void(SBAPI *pAdmin)(void* obj, bool admin);
typedef void(SBAPI *pSlider)(void* obj);
typedef void(SBAPI *pOptionSave)(void* obj);

// Original func vars
pBack pOrigBack = NULL;
pToggleDebug pOrigToggleDebug = NULL;
pInvBar oInvBar = NULL;
pZoom oZoom = NULL;
pInventory oInventory = NULL;
pAdmin oAdmin = NULL;
pSlider oSlider = NULL;
pOptionSave oOptionSlave = NULL;

// Our funcs
void SML toggleDebug(void* obj, void* u);
void SML back(void* obj, void* u);
void SML invBar(void* obj, void* u);
void SML zoom(void* obj, void* u);
void SML inventory(void *obj, void* u);
void SML admin(void* obj, void* u, bool admin);
void SML slider(void* obj, void* u);
void SML saveOptions(void* obj, void* u);

void getHwnd();
HWND sbHwnd;
BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam);
void hook();
void unhook();
HMODULE GetCurrentModule();
void msg(std::string);
void msg(int);
HANDLE sbProc;

void* mainInterface;
void* player;
BOOL inZoomUpdate = FALSE;

BOOL APIENTRY DllMain(HMODULE hDLL, DWORD reason, LPVOID reserved)
{

	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
	{
							   getHwnd();
							   sbProc = GetCurrentProcess();
							   char buffer[256];
							   int length = 0;
							   length = GetWindowTextLengthA(sbHwnd);
							   GetWindowTextA(sbHwnd, buffer, length + 1);
							   SetWindowTextA(sbHwnd, (std::string(buffer) + " - aevv mod loader").c_str());

							   hook();
							   break;
	}
	case DLL_PROCESS_DETACH:
	{
							   unhook();
							   break;
	}
	}

	return TRUE;
}

void hook()
{
	HMODULE proc = GetCurrentModule();

	pOrigBack = (pBack)0x00411B70;
	Mhook_SetHook((PVOID*)&pOrigBack, back);

	pOrigToggleDebug = (pToggleDebug)0x00427BD0;
	Mhook_SetHook((PVOID*)&pOrigToggleDebug, toggleDebug);

	oInvBar = (pInvBar)0x00427CF0;
	Mhook_SetHook((PVOID*)&oInvBar, invBar);

	oZoom = (pZoom)0x0043C420;
	Mhook_SetHook((PVOID*)&oZoom, zoom);

	oInventory = (pInventory)0x0056C7A0;
	Mhook_SetHook((PVOID*)&oInventory, inventory);

	oAdmin = (pAdmin)0x0056B750;
	Mhook_SetHook((PVOID*)&oAdmin, admin);

	oSlider = (pSlider)0x004F65A0;
	Mhook_SetHook((PVOID*)&oSlider, slider);

	oOptionSlave = (pOptionSave)0x00440AD0;
	Mhook_SetHook((PVOID*)&oOptionSlave, saveOptions);
}

void unhook()
{

}

// Hooked funcs

void SML back(void* obj, void* u)
{
	pOrigBack(obj);
}

void SML toggleDebug(void* obj, void* u)
{
	pOrigToggleDebug(obj);
}

void SML invBar(void* obj, void* u)
{
	if (!mainInterface)
	{
		mainInterface = obj;
		pOrigToggleDebug(obj);
	}
	oInvBar(obj);
}

void SML zoom(void* obj, void* u)
{
	inZoomUpdate = TRUE;
	oZoom(obj);
	inZoomUpdate = FALSE;
}

void SML inventory(void* obj, void* u)
{
	if (!player)
	{
		player = obj;
	}
	oInventory(obj);
}

void SML admin(void* obj, void* u, bool admin)
{
	oAdmin(obj, true);
}

void* sliderAddr;

void SML saveOptions(void* obj, void* u)
{
	LPVOID addr = (LPVOID)((DWORD)sliderAddr + 0x00E0);
	DWORD value;
	ReadProcessMemory(sbProc, addr, &value, sizeof(value), NULL);
	msg(value);
	value = value - 1;
	msg(value);
	WriteProcessMemory(sbProc, addr, (LPVOID)value, 1, NULL);
}

void SML slider(void* obj, void* u)
{
	if (inZoomUpdate)
	{
		inZoomUpdate = FALSE;
		sliderAddr = obj;
	}

	if (sliderAddr == obj)
	{
		LPVOID addr = (LPVOID)((DWORD)sliderAddr + 0x00E0);
		DWORD* value = (DWORD*)addr;
		msg(*value);
		*value++;
		msg(*value);
		WriteProcessMemory(sbProc, addr, (LPVOID)value, 1, NULL);
	}

	oSlider(obj);
}

// Utility

void getHwnd()
{
	EnumWindows(enumWindowsProc, 0);
}

BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam)
{
	char buffer[256];

	int length = GetWindowTextLength(hwnd);
	if (length > 0)
	{
		GetWindowTextA(hwnd, buffer, length + 1);

		std::string window = std::string(buffer);
		if (window == "Starbound - Beta")
		{
			sbHwnd = hwnd;
		}
	}
	return TRUE;
}

HMODULE GetCurrentModule()
{
	HMODULE hModule = NULL;
	GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)GetCurrentModule, &hModule);

	return hModule;
}

void msg(std::string text)
{
	MessageBoxA(sbHwnd, text.c_str(), "SML", MB_ICONINFORMATION);
}

void msg(int number)
{
	std::ostringstream stream;
	stream << number;
	msg(stream.str());
}