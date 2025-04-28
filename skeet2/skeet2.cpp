#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <cstdio>
#include <optional>

#include <Windows.h>
#include <TlHelp32.h>

#include "path_finder.hpp"

static int autoStart(const std::string& additionalArgs = "")
{
    const char* lpGameName = "Counter-Strike Global Offensive";
    const char* lpGameArgs = "-steam -insecure";
    const char* lpProcessName  = "csgo.exe";
    AllowGamePathCaching = false;

    std::string fullArgs = lpGameArgs;
    if (!additionalArgs.empty()) {
        fullArgs += " " + additionalArgs;
    }
    
    auto gamePath = GetSteamGamePath(lpGameName, lpProcessName);
    if (!gamePath) {
        printf("Game '%s' not found.\n", lpGameName);
        return -1;
    }

    std::string exePath = (gamePath.value() / lpProcessName).string();
    HINSTANCE result = ShellExecuteA(NULL, "open", exePath.c_str(), fullArgs.c_str(), NULL, SW_SHOWNORMAL);

    if ((INT_PTR)result <= 32) {
        printf("Failed to start game. Error code: %ld\n", (long)result);
        return -1;
    }

    return 0;
}


static DWORD GetProcessByName(const char* lpProcessName)
{
    char lpCurrentProcessName[255];
    PROCESSENTRY32 ProcList{};
    ProcList.dwSize = sizeof(ProcList);
    const HANDLE hProcList = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hProcList == INVALID_HANDLE_VALUE) return -1;
    if (!Process32First(hProcList, &ProcList)) return -1;

    wcstombs_s(nullptr, lpCurrentProcessName, ProcList.szExeFile, 255);
    if (lstrcmpA(lpCurrentProcessName, lpProcessName) == 0) return ProcList.th32ProcessID;

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

    std::string additionalArgs; // Added the ability to accept additional arguments for game
    for (int i = 1; i < argc; ++i) {
        if (i > 1) additionalArgs += " ";
        additionalArgs += argv[i];
    }

    printf("Waiting for process: %s\n", lpProcessName);

    DWORD dwProcessID = GetProcessByName(lpProcessName); // Small changes in game process finding 
    if (dwProcessID == (DWORD)-1) {
        printf("Game not running, trying auto start...\n");
        autoStart(additionalArgs);
        do {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            dwProcessID = GetProcessByName(lpProcessName);
        } while (dwProcessID == (DWORD)-1);
    }

    printf("Process found!\nProcess ID: %i\n\n", (int)dwProcessID);

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

    printf("Process injection...\n");
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
