#undef UNICODE
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>
#include <cstdio>
#include "mhook-lib\mhook.h"

#define SBAPI __thiscall
#define SML __fastcall


// Hooks
typedef void(SBAPI *pToggleDebug)(void* obj);
typedef void(SBAPI *pBack)(void* obj);
typedef void(SBAPI *pInvBar)(void* obj);
typedef void(SBAPI *pZoom)(void* obj);

// Original func vars
pBack pOrigBack = NULL;
pToggleDebug pOrigToggleDebug = NULL;
pInvBar oInvBar = NULL;
pZoom oZoom = NULL;

// Our funcs
void SML toggleDebug(void* obj, void* u);
void SML back(void* obj, void* u);
void SML invBar(void* obj, void* u);
void SML zoom(void* obj, void* u);

void getHwnd();
HWND sbHwnd;
BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam);
void hook();
void unhook();
HMODULE GetCurrentModule();
void msg(std::string);

void* mainInterface;

BOOL APIENTRY DllMain(HMODULE hDLL, DWORD reason, LPVOID reserved)
{

	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
	{
							   getHwnd();
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
	oInvBar(obj);
}

void SML zoom(void* obj, void* u)
{
	msg((char*)obj);
	oZoom(obj);
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