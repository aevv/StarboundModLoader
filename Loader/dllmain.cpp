#undef UNICODE
#define WIN32_LEAN_AND_MEAN

#include <metahost.h>
#include <string>
#include <sstream>
#include "mhook-lib\mhook.h"

#pragma comment(lib, "mscoree.lib")

#import "mscorlib.tlb" raw_interfaces_only				\
	high_property_prefixes("_get", "_put", "_putref")		\
	rename("ReportEvent", "InteropServices_ReportEvent")

using namespace mscorlib;

#define SBAPI __thiscall
#define SML __fastcall

typedef void(SBAPI *pAdmin)(void* obj, bool admin);
pAdmin oAdmin = NULL;
void SML admin(void* obj, void* u, bool admin);

BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam);
void getHwnd();
void msg(std::string);
void msg(int);

wchar_t* assembly = L"D:\\Code\\SML\\StarboundModLoader\\TestModule\\bin\\Debug\\TestModule.dll";
wchar_t* type = L"TestModule.Class1";
wchar_t* method = L"Entry";
wchar_t* arg = L"";
HWND sbHwnd;
HANDLE sbProc;

BOOL isAdmin = FALSE;

extern "C" __declspec(dllexport) void testCallback(int admin)
{
	isAdmin = admin;
}

__declspec(dllexport) HRESULT ImplantDotNetAssembly(_In_ LPCTSTR lpCommand)
{
	HRESULT hr;
	ICLRMetaHost *pMetaHost = NULL;
	ICLRRuntimeInfo *pRuntimeInfo = NULL;
	ICLRRuntimeHost *pClrRuntimeHost = NULL;

	hr = CLRCreateInstance(CLSID_CLRMetaHost, IID_PPV_ARGS(&pMetaHost));
	hr = pMetaHost->GetRuntime(L"v4.0.30319", IID_PPV_ARGS(&pRuntimeInfo));
	hr = pRuntimeInfo->GetInterface(CLSID_CLRRuntimeHost, IID_PPV_ARGS(&pClrRuntimeHost));

	hr = pClrRuntimeHost->Start();

	DWORD pReturnValue;

	int ptr;
	ptr = (int)&testCallback;
	WCHAR param[5];
	param[0] = 0xFF & (ptr >> 0);
	param[1] = 0xFF & (ptr >> 8);
	param[2] = 0xFF & (ptr >> 16);
	param[3] = 0xFF & (ptr >> 24);
	param[4] = '\0';

	hr = pClrRuntimeHost->ExecuteInDefaultAppDomain(
		assembly,
		type,
		method,
		(LPCWSTR)param,
		&pReturnValue);

	pMetaHost->Release();
	pRuntimeInfo->Release();
	pClrRuntimeHost->Release();

	return hr;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
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

							   /*oAdmin = (pAdmin)0x0056B750;
							   Mhook_SetHook((PVOID*)&oAdmin, admin);*/
	}
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

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

void SML admin(void* obj, void* u, bool admin)
{
	oAdmin(obj, admin);
}