#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <Windows.h>
#include <TlHelp32.h>
#include "game_starter.h"

int startGame()
{
    std::string steamPath = findSteamPath();
    // my csgo arguments personally, modify them to suit your own
    // -steam argument is required for the game to start properly
    const char* gameArgs = "-steam -insecure -novid -d3d9ex -console -freq 144"
                            "-high +rate 128000 +cl_cmdrate 128 +cl_updaterate 128"
                            " -tickrate 128 +ex_interpratio 1 +cl_interp 0.01"
                            " -noforcemspd -noforcemaccel -noforcemparms -threads 6 -nojoy";

    if (steamPath.empty()) {
        printf("failed to find steam\n");
        return -1;
    } else {
        printf("steam found");
    }
    
    std::string csgoPath = findGamePath(steamPath, "Counter-Strike Global Offensive");

    if (csgoPath.empty()) {
        printf("CS:GO not found\n");
        return -1;
    } else {
        printf("CSGO found");
    }
    
    std::string fullGamePath = csgoPath + "\\csgo.exe";
    const char* gamePath = fullGamePath.c_str();
    HINSTANCE result = ShellExecuteA(NULL, "open", gamePath, gameArgs, NULL, SW_SHOWNORMAL);
    
    if ((int)result <= 32) {
        MessageBoxA(NULL, "failed to start CS:GO directly.", "Game Start Error", MB_ICONERROR);
        return -1;
    }
}

DWORD GetProcessByName(const char* lpProcessName)
{
    char lpCurrentProcessName[255];

    PROCESSENTRY32 ProcList{};
    ProcList.dwSize = sizeof(ProcList);

    const HANDLE hProcList = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcList == INVALID_HANDLE_VALUE)
        return -1;

    if (!Process32First(hProcList, &ProcList))
        return -1;

    wcstombs_s(nullptr, lpCurrentProcessName, ProcList.szExeFile, 255);

    if (lstrcmpA(lpCurrentProcessName, lpProcessName) == 0)
        return ProcList.th32ProcessID;

    while (Process32Next(hProcList, &ProcList))
    {
        wcstombs_s(nullptr, lpCurrentProcessName, ProcList.szExeFile, 255);

        if (lstrcmpA(lpCurrentProcessName, lpProcessName) == 0)
            return ProcList.th32ProcessID;
    }

    return -1;
}
// dll injector by https://github.com/adamhlt/DLL-Injector
int main(const int argc, char* argv[])
{
    const char* lpDLLName = "skeet.dll";
    const char* lpProcessName = "csgo.exe";
    char lpFullDLLPath[MAX_PATH];
    printf("Waiting for process: %s\n", lpProcessName);

    startGame();

    DWORD dwProcessID = (DWORD)-1;
    while (dwProcessID == (DWORD)-1)
    {
        dwProcessID = GetProcessByName(lpProcessName);
        if (dwProcessID == (DWORD)-1)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    printf("Process found!\n");
    printf("Process ID: %i\n\n", (int)dwProcessID);

    const DWORD dwFullPathResult = GetFullPathNameA(lpDLLName, MAX_PATH, lpFullDLLPath, nullptr);
    if (dwFullPathResult == 0)
    {
        printf("An error occurred when trying to get the full path of the DLL.\n");
        return -1;
    }

    const HANDLE hTargetProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwProcessID);
    if (hTargetProcess == INVALID_HANDLE_VALUE)
    {
        printf("An error occurred when trying to open the target process.\n");
        return -1;
    }

    printf("[PROCESS INJECTION]\n");
    printf("Process opened successfully.\n");

    VirtualAllocEx(hTargetProcess, (LPVOID)0x43310000, 0x2FC000u, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE); // for skeet
    VirtualAllocEx(hTargetProcess, 0, 0x1000u, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE); // for skeet

    const LPVOID lpPathAddress = VirtualAllocEx(hTargetProcess, nullptr, lstrlenA(lpFullDLLPath) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (lpPathAddress == nullptr)
    {
        printf("An error occurred when trying to allocate memory in the target process.\n");
        return -1;
    }

    printf("Memory allocated at 0x%X\n", (UINT)(uintptr_t)lpPathAddress);

    const DWORD dwWriteResult = WriteProcessMemory(hTargetProcess, lpPathAddress, lpFullDLLPath, lstrlenA(lpFullDLLPath) + 1, nullptr);
    if (dwWriteResult == 0)
    {
        printf("An error occurred when trying to write the DLL path in the target process.\n");
        return -1;
    }

    printf("DLL path written successfully.\n");

    const HMODULE hModule = GetModuleHandleA("kernel32.dll");
    if (hModule == INVALID_HANDLE_VALUE || hModule == nullptr)
        return -1;

    const FARPROC lpFunctionAddress = GetProcAddress(hModule, "LoadLibraryA");
    if (lpFunctionAddress == nullptr)
    {
        printf("An error occurred when trying to get \"LoadLibraryA\" address.\n");
        return -1;
    }

    printf("LoadLibraryA address at 0x%X\n", (UINT)(uintptr_t)lpFunctionAddress);

    const HANDLE hThreadCreationResult = CreateRemoteThread(hTargetProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)lpFunctionAddress, lpPathAddress, 0, nullptr);
    if (hThreadCreationResult == INVALID_HANDLE_VALUE)
    {
        printf("An error occurred when trying to create the thread in the target process.\n");
        return -1;
    }

    printf("DLL Injected!\n");

    return 0;
}
