#include <string>
#include "windows.h"
#include "Shlwapi.h"
#include "TlHelp32.h"

#pragma comment(lib, "shlwapi")

#pragma region constants

static const wchar_t* LOADER_DLL = L"Loader.dll";

static const std::wstring NOTICE =
std::wstring(L"\n\nInject v1.0 - Inject managed code into unmanaged and managed applications.\n") +
std::wstring(L"Copyright (C) 2013 Pero Matic\n") +
std::wstring(L"Plan A Software - www.planasoftware.com\n\n");

static const wchar_t* DEDICATIONS = L"This app is dedicated to my brother Milan.\n\n";

#pragma endregion

// prototypes
DWORD_PTR inject(const HANDLE hProcess, const LPVOID function, const std::wstring& argument);
DWORD_PTR getFunctionOffset(const std::wstring& library, const char* functionName);
int getProcessIdByName(wchar_t* processName);
DWORD_PTR getRemoteModuleHandle(const int processId, const wchar_t* moduleName);
void enablePrivilege(const wchar_t* lpPrivilegeName, const bool bEnable);
std::wstring getLoaderPath();
BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam); 
inline size_t getStringAllocSize(const std::wstring& str);

// global variables
HWND sbHwnd;

int wmain(int argc, wchar_t* argv[])
{
	EnumWindows(enumWindowsProc, 0);

	// enable debug privileges
	enablePrivilege(SE_DEBUG_NAME, TRUE);

	DWORD procId = 0;
	GetWindowThreadProcessId(sbHwnd, &procId);
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, procId);

	FARPROC fnLoadLibrary = GetProcAddress(GetModuleHandle(L"Kernel32"), "LoadLibraryW");
	inject(hProcess, fnLoadLibrary, getLoaderPath());	

	DWORD_PTR hBootstrap = getRemoteModuleHandle(procId, LOADER_DLL);
	DWORD_PTR offset = getFunctionOffset(getLoaderPath(), "ImplantDotNetAssembly");
	DWORD_PTR fnImplant = hBootstrap + offset;

	std::wstring argument = L"";

	inject(hProcess, (LPVOID)fnImplant, argument);

	FARPROC fnFreeLibrary = GetProcAddress(GetModuleHandle(L"Kernel32"), "FreeLibrary");
	CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)fnFreeLibrary, (LPVOID)hBootstrap, NULL, 0);

	CloseHandle(hProcess);

	return 0;
}

DWORD_PTR inject(const HANDLE hProcess, const LPVOID function, const std::wstring& argument)
{
	// allocate some memory in remote process
	LPVOID baseAddress = VirtualAllocEx(hProcess, NULL, getStringAllocSize(argument), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

	// write argument into remote process	
	BOOL isSucceeded = WriteProcessMemory(hProcess, baseAddress, argument.c_str(), getStringAllocSize(argument), NULL);

	// make the remote process invoke the function
	HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)function, baseAddress, NULL, 0);

	// wait for thread to exit
	WaitForSingleObject(hThread, INFINITE);

	// free memory in remote process
	VirtualFreeEx(hProcess, baseAddress, 0, MEM_RELEASE);

	// get the thread exit code
	DWORD exitCode = 0;
	GetExitCodeThread(hThread, &exitCode);

	// close thread handle
	CloseHandle(hThread);

	// return the exit code
	return exitCode;
}

DWORD_PTR getFunctionOffset(const std::wstring& library, const char* functionName)
{
	// load library into this process
	HMODULE hLoaded = LoadLibrary(library.c_str());

	// get address of function to invoke
	void* lpInject = GetProcAddress(hLoaded, functionName);

	// compute the distance between the base address and the function to invoke
	DWORD_PTR offset = (DWORD_PTR)lpInject - (DWORD_PTR)hLoaded;

	// unload library from this process
	FreeLibrary(hLoaded);

	// return the offset to the function
	return offset;
}

int getProcessIdByName(wchar_t* processName)
{
	PROCESSENTRY32 pe32;
	HANDLE hSnapshot = INVALID_HANDLE_VALUE;

	// get snapshot of all processes
	pe32.dwSize = sizeof(PROCESSENTRY32);
	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	// can we start looking?
	if (!Process32First(hSnapshot, &pe32))
	{
		CloseHandle(hSnapshot);
		return 0;
	}

	// enumerate all processes till we find the one we are looking for or until every one of them is checked
	while (wcscmp(pe32.szExeFile, processName) != 0 && Process32Next(hSnapshot, &pe32));

	// close the handle
	CloseHandle(hSnapshot);

	// check if process id was found and return it
	if (wcscmp(pe32.szExeFile, processName) == 0)
		return pe32.th32ProcessID;

	return 0;
}

DWORD_PTR getRemoteModuleHandle(const int processId, const wchar_t* moduleName)
{
	MODULEENTRY32 me32;
	HANDLE hSnapshot = INVALID_HANDLE_VALUE;

	// get snapshot of all modules in the remote process 
	me32.dwSize = sizeof(MODULEENTRY32);
	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId);

	// can we start looking?
	if (!Module32First(hSnapshot, &me32))
	{
		CloseHandle(hSnapshot);
		return 0;
	}

	// enumerate all modules till we find the one we are looking for or until every one of them is checked
	while (wcscmp(me32.szModule, moduleName) != 0 && Module32Next(hSnapshot, &me32));

	// close the handle
	CloseHandle(hSnapshot);

	// check if module handle was found and return it
	if (wcscmp(me32.szModule, moduleName) == 0)
		return (DWORD_PTR)me32.modBaseAddr;

	return 0;
}


void enablePrivilege(const wchar_t* lpPrivilegeName, const bool enable)
{
	HANDLE token;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
		return;

	TOKEN_PRIVILEGES privileges;
	ZeroMemory(&privileges, sizeof(privileges));
	privileges.PrivilegeCount = 1;
	privileges.Privileges[0].Attributes = (enable) ? SE_PRIVILEGE_ENABLED : 0;
	if (!LookupPrivilegeValue(NULL, lpPrivilegeName, &privileges.Privileges[0].Luid))
	{
		CloseHandle(token);
		return;
	}

	BOOL result = AdjustTokenPrivileges(token, FALSE, &privileges, sizeof(privileges), NULL, NULL);

	CloseHandle(token);
}

std::wstring getLoaderPath()
{
	wchar_t buffer[MAX_PATH];
	GetModuleFileName(NULL, buffer, MAX_PATH);

	PathRemoveFileSpec(buffer);

	size_t size = wcslen(buffer) + wcslen(LOADER_DLL);
	if (size <= MAX_PATH)
		PathAppend(buffer, LOADER_DLL);
	else
		throw std::runtime_error("Module name cannot exceed MAX_PATH");

	return buffer;
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

inline size_t getStringAllocSize(const std::wstring& str)
{
	// size (in bytes) of string
	return (wcsnlen(str.c_str(), 65536) * sizeof(wchar_t)) + sizeof(wchar_t); // +sizeof(wchar_t) for null
}


//#include <Windows.h>
//#include <string>
//#include <iostream>
//#include "TlHelp32.h"
//#include "Shlwapi.h"
//#include "windows.h"
//
//#pragma comment(lib, "Shlwapi.lib")
//
//char* testHook = "D:\\Code\\SML\\StarboundModLoader\\Debug\\TestHook.dll";
//wchar_t* smlLoader = L"D:\\Code\\SML\\StarboundModLoader\\Loader\\Debug\\Loader.dll";
//HWND sbHwnd;
//
//BOOL injectUnmanaged(HWND hwnd, char*);
//BOOL injectManaged(HWND hwnd, char*);
//BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam);
//void pause();
//
//std::string getModulePath(char* module);
//DWORD_PTR getRemoteHandle(int procId, char* moduleName);
//DWORD_PTR getFuncOffset(std::string& library, char* func);
//BOOL implant(HWND, LPVOID, LPVOID);
//
//inline size_t GetStringAllocSize(const std::wstring& str)
//{
//	// size (in bytes) of string
//	return (wcsnlen(str.c_str(), 65536) * sizeof(wchar_t)) + sizeof(wchar_t); // +sizeof(wchar_t) for null
//}
//
//DWORD_PTR Inject(const HANDLE hProcess, const LPVOID function, const std::wstring& argument)
//{
//	// allocate some memory in remote process
//	LPVOID baseAddress = VirtualAllocEx(hProcess, NULL, GetStringAllocSize(argument), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
//
//	// write argument into remote process	
//	BOOL isSucceeded = WriteProcessMemory(hProcess, baseAddress, argument.c_str(), GetStringAllocSize(argument), NULL);
//
//	// make the remote process invoke the function
//	HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)function, baseAddress, NULL, 0);
//
//	// wait for thread to exit
//	WaitForSingleObject(hThread, INFINITE);
//
//	// free memory in remote process
//	VirtualFreeEx(hProcess, baseAddress, 0, MEM_RELEASE);
//
//	// get the thread exit code
//	DWORD exitCode = 0;
//	GetExitCodeThread(hThread, &exitCode);
//
//	// close thread handle
//	CloseHandle(hThread);
//
//	// return the exit code
//	return exitCode;
//}
//
//int main(int argc, char *argv[], char *envp[])
//{	
//	EnumWindows(enumWindowsProc, 0);
//
//	if (sbHwnd == NULL)
//	{
//		std::cout << "Starbound not found: " << GetLastError() << std::endl;
//		pause();
//		return 1;
//	}
//
//	//std::cout << "Starbound found, attempting to inject " << smlLoader << std::endl;
//
//	//if (injectManaged(sbHwnd, smlLoader) == TRUE)
//	//{
//	//	std::cout << "Success!" << std::endl;
//	//}
//	DWORD procId = 0;
//	GetWindowThreadProcessId(sbHwnd, &procId);
//	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, procId);
//
//	FARPROC fnLoadLibrary = GetProcAddress(GetModuleHandleW(L"Kernel32"), "LoadLibraryW");
//	Inject(hProcess, fnLoadLibrary, smlLoader);
//
//	DWORD_PTR loaderPtr = getRemoteHandle(procId, "Loader.dll");
//	DWORD_PTR offset = getFuncOffset(getModulePath("D:\\Code\\SML\\StarboundModLoader\\Loader\\Debug\\Loader.dll"), "implantSML");
//	DWORD_PTR implantFunc = loaderPtr + offset;
//
//	Inject(hProcess, (LPVOID)implantFunc, L"D:\\Code\\SML\\StarboundModLoader\\TestModule\\bin\\Debug\\TestModule.dll\tTestModule.Class1\tEntry\tTest");
//
//	return 0;
//}
//
//BOOL injectManaged(HWND hwnd, char* loader)
//{
//	if (injectUnmanaged(hwnd, loader) == FALSE)
//		return FALSE;
//
//	DWORD procId = 0;
//	GetWindowThreadProcessId(hwnd, &procId);
//
//	DWORD_PTR loaderPtr = getRemoteHandle(procId, "Loader.dll");
//	DWORD_PTR offset = getFuncOffset(getModulePath(loader), "implantSML");
//	DWORD_PTR implantFunc = loaderPtr + offset;
//
//	if (implant(hwnd, (LPVOID)implantFunc, (LPVOID)loaderPtr) == FALSE)
//		return false;
//
//	return TRUE;
//}
//
//BOOL implant(HWND hwnd, LPVOID function, LPVOID loader)
//{
//	DWORD processId;
//	GetWindowThreadProcessId(hwnd, &processId);
//	HANDLE handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
//
//	// allocate some memory in remote process
//	LPVOID baseAddress = VirtualAllocEx(handle, NULL, sizeof(wchar_t), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
//
//	// make the remote process invoke the function
//	HANDLE hThread = CreateRemoteThread(handle, NULL, 0, (LPTHREAD_START_ROUTINE)function, baseAddress, NULL, 0);
//
//	// wait for thread to exit
//	WaitForSingleObject(hThread, INFINITE);
//
//	// free memory in remote process
//	VirtualFreeEx(handle, baseAddress, 0, MEM_RELEASE);
//
//	// get the thread exit code
//	DWORD exitCode = 0;
//	GetExitCodeThread(hThread, &exitCode);
//
//	// close thread handle
//	CloseHandle(hThread);
//
//	FARPROC fnFreeLibrary = GetProcAddress(GetModuleHandle("Kernel32"), "FreeLibraryA");
//	CreateRemoteThread(handle, NULL, 0, (LPTHREAD_START_ROUTINE)fnFreeLibrary, (LPVOID)loader, NULL, 0);
//
//	// close process handle
//	CloseHandle(handle);
//
//	// return the exit code
//	return TRUE;
//}
//
//BOOL injectUnmanaged(HWND hwnd, char* dll)
//{
//	DWORD processId;
//	GetWindowThreadProcessId(hwnd, &processId);
//	HANDLE handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
//
//	if (!handle)
//	{
//		std::cout << "Could not open process" << std::endl;
//		return FALSE;
//	}
//
//	LPVOID loader = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
//
//	if (!loader || GetLastError() > 0)
//	{
//		std::cout << "Could not get process address for LoadLibraryA" << std::endl;
//		return FALSE;
//	}
//
//	LPVOID addr = VirtualAllocEx(handle, NULL, strlen(dll) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
//
//	if (addr == NULL || GetLastError() > 0)
//	{
//		std::cout << "Could not allocate memory" << std::endl;
//		return FALSE;
//	}
//
//	BOOL write = WriteProcessMemory(handle, addr, (void*)dll, strlen(dll) + 1, NULL);
//
//	if (write == FALSE || GetLastError() > 0)
//	{
//		std::cout << "Could not write hook into process" << std::endl;
//		return FALSE;
//	}
//
//	HANDLE remoteThread = CreateRemoteThread(handle, NULL, NULL, (LPTHREAD_START_ROUTINE)loader, addr, 0, NULL);
//
//	if (!remoteThread || GetLastError() > 0)
//	{
//		std::cout << "Could not create remote thread" << std::endl;
//		return FALSE;
//	}
//
//	WaitForSingleObject(remoteThread, INFINITE);
//
//	HMODULE library = NULL;
//	BOOL exitCode = GetExitCodeThread(remoteThread, (LPDWORD)&library);
//
//	if (exitCode == FALSE || GetLastError() > 0)
//	{
//		std::cout << "Could not get exit code" << std::endl;
//		return FALSE;
//	}
//
//	BOOL closeRemote = CloseHandle(remoteThread);
//
//	if (closeRemote == FALSE || GetLastError() > 0)
//	{
//		std::cout << "Could not close remote thread" << std::endl;
//		return FALSE;
//	}
//
//	BOOL free = VirtualFreeEx(handle, addr, 0, MEM_RELEASE);
//
//	if (free == FALSE || GetLastError() > 0)
//	{
//		std::cout << "Could not free memory: " << GetLastError() << std::endl;
//		return FALSE;
//	}
//
//	BOOL closeHandle = CloseHandle(handle);
//
//	if (closeHandle == FALSE || GetLastError() > 0)
//	{
//		std::cout << "Could not close handle" << std::endl;
//		return FALSE;
//	}
//
//	return TRUE;
//}
//
//DWORD_PTR getRemoteHandle(int procId, char* moduleName)
//{
//	MODULEENTRY32 me32;
//	HANDLE hSnapshot = INVALID_HANDLE_VALUE;
//
//	me32.dwSize = sizeof(MODULEENTRY32);
//	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, procId);
//
//	if (!Module32First(hSnapshot, &me32))
//	{
//		CloseHandle(hSnapshot);
//		return 0;
//	}
//
//	while (strcmp(me32.szModule, moduleName) != 0 && Module32Next(hSnapshot, &me32));
//
//	CloseHandle(hSnapshot);
//
//	if (strcmp(me32.szModule, moduleName) == 0)
//		return (DWORD_PTR)me32.modBaseAddr;
//
//	return 0;
//}
//
//DWORD_PTR getFuncOffset(std::string& library, char* functionName)
//{
//	HMODULE hLoaded = LoadLibrary(library.c_str());
//
//	void* lpInject = GetProcAddress(hLoaded, functionName);
//
//	DWORD_PTR offset = (DWORD_PTR)lpInject - (DWORD_PTR)hLoaded;
//
//	FreeLibrary(hLoaded);
//
//	return offset;
//}
//
//BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam)
//{
//	char buffer[256];
//
//	int length = GetWindowTextLength(hwnd);
//	if (length > 0)
//	{
//		GetWindowTextA(hwnd, buffer, length + 1);
//
//		std::string window = std::string(buffer);
//		if (window == "Starbound - Beta")
//		{
//			sbHwnd = hwnd;
//		}
//	}
//	return TRUE;
//}
//
//void pause()
//{
//	std::cout << "Press any key to continue" << std::endl;
//	std::getchar();
//}
//
//std::string getModulePath(char* module)
//{
//	// get full path to exe
//	char buffer[MAX_PATH];
//	GetModuleFileName(NULL, buffer, MAX_PATH);
//
//	// remove filename
//	PathRemoveFileSpec(buffer);
//
//	// append bootstrap to buffer if it will fit
//	size_t size = strlen(buffer) + strlen(module);
//	if (size <= MAX_PATH)
//		PathAppend(buffer, module);
//
//	// return path
//	return buffer;
//}
