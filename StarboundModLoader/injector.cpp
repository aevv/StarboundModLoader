#include <Windows.h>
#include <string>
#include <iostream>
#include "injector.h"

char* file = "D:\\Code\\SML\\TestHook\\Debug\\TestHook.dll";
HWND sbHwnd;

BOOL inject(HWND hwnd);
BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam);
void pause();

int main(int argc, char *argv[], char *envp[])
{	
	EnumWindows(enumWindowsProc, 0);

	if (sbHwnd == NULL)
	{
		std::cout << "Starbound not found: " << GetLastError() << std::endl;
		pause();
		return 1;
	}

	std::cout << "Starbound found, attempting to inject " << file << std::endl;

	if (inject(sbHwnd) == TRUE)
	{
		std::cout << "Success!" << std::endl;
	}

	return 0;
}

BOOL inject(HWND hwnd)
{
	DWORD processId;
	GetWindowThreadProcessId(hwnd, &processId);
	HANDLE handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);

	if (!handle)
	{
		std::cout << "Could not open process" << std::endl;
		return FALSE;
	}

	LPVOID loader = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");

	if (!loader || GetLastError() > 0)
	{
		std::cout << "Could not get process address for LoadLibraryA" << std::endl;
		return FALSE;
	}

	LPVOID addr = VirtualAllocEx(handle, NULL, strlen(file) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

	if (addr == NULL || GetLastError() > 0)
	{
		std::cout << "Could not allocate memory" << std::endl;
		return FALSE;
	}

	BOOL write = WriteProcessMemory(handle, addr, (void*)file, strlen(file) + 1, NULL);

	if (write == FALSE || GetLastError() > 0)
	{
		std::cout << "Could not write hook into process" << std::endl;
		return FALSE;
	}

	HANDLE remoteThread = CreateRemoteThread(handle, NULL, NULL, (LPTHREAD_START_ROUTINE)loader, addr, 0, NULL);

	if (!remoteThread || GetLastError() > 0)
	{
		std::cout << "Could not create remote thread" << std::endl;
		return FALSE;
	}

	WaitForSingleObject(remoteThread, INFINITE);

	HMODULE library = NULL;
	BOOL exitCode = GetExitCodeThread(remoteThread, (LPDWORD)&library);

	if (exitCode == FALSE || GetLastError() > 0)
	{
		std::cout << "Could not get exit code" << std::endl;
		return FALSE;
	}

	BOOL closeRemote = CloseHandle(remoteThread);

	if (closeRemote == FALSE || GetLastError() > 0)
	{
		std::cout << "Could not close remote thread" << std::endl;
		return FALSE;
	}

	BOOL free = VirtualFreeEx(handle, addr, 0, MEM_RELEASE);

	if (free == FALSE || GetLastError() > 0)
	{
		std::cout << "Could not free memory: " << GetLastError() << std::endl;
		return FALSE;
	}

	BOOL closeHandle = CloseHandle(handle);

	if (closeHandle == FALSE || GetLastError() > 0)
	{
		std::cout << "Could not close handle" << std::endl;
		return FALSE;
	}

	return TRUE;
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

void pause()
{
	std::cout << "Press any key to continue" << std::endl;
	std::getchar();
}