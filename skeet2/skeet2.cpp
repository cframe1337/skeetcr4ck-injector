#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <Windows.h>
#include <TlHelp32.h>
#include <ShellApi.h>
#include <Shlwapi.h>

#pragma comment(lib, "Shlwapi.lib")

LPVOID ntOpenFile = GetProcAddress(LoadLibraryW(L"ntdll"), "NtOpenFile"); // https://github.com/v3ctra/load-lib-injector

const char* stringToConstChar(const std::string& str) {
    return str.c_str();
}

std::vector<std::string> getAvailableDrives() {
    std::vector<std::string> drives;
    char buffer[256];
    DWORD length = GetLogicalDriveStringsA(sizeof(buffer), buffer);

    if (length > 0 && length <= sizeof(buffer)) {
        char* drive = buffer;
        while (*drive) {
            if (GetDriveTypeA(drive) == DRIVE_FIXED) {
                drives.push_back(drive);
            }
            drive += strlen(drive) + 1;
        }
    }

    return drives;
}

bool directoryExists(const std::string& path) {
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
}

bool fileExists(const std::string& path) {
    return PathFileExistsA(path.c_str()) && !PathIsDirectoryA(path.c_str());
}

std::vector<std::string> findDirectories(const std::string& path) {
    std::vector<std::string> directories;
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA((path + "\\*").c_str(), &findData);

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                strcmp(findData.cFileName, ".") != 0 &&
                strcmp(findData.cFileName, "..") != 0) {
                directories.push_back(path + "\\" + findData.cFileName);
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }

    return directories;
}

std::string findSteamPath() {
    std::vector<std::string> drives = getAvailableDrives();

    for (const auto& drive : drives) {
        std::vector<std::string> commonLocations = {
            drive + "Program Files (x86)\\Steam",
            drive + "Program Files\\Steam",
            drive + "Steam"
        };

        for (const auto& path : commonLocations) {
            if (directoryExists(path) && fileExists(path + "\\steam.exe")) {
                return path;
            }
        }
    }

    for (const auto& drive : drives) {
        std::vector<std::string> rootDirs = findDirectories(drive);

        for (const auto& dir : rootDirs) {
            if (fileExists(dir + "\\steam.exe")) {
                return dir;
            }

            std::string dirName = dir.substr(dir.find_last_of('\\') + 1);
            if (dirName == "Steam" && fileExists(dir + "\\steam.exe")) {
                return dir;
            }

            if (dirName == "Program Files" || dirName == "Program Files (x86)") {
                std::vector<std::string> subDirs = findDirectories(dir);
                for (const auto& subDir : subDirs) {
                    std::string subDirName = subDir.substr(subDir.find_last_of('\\') + 1);
                    if (subDirName == "Steam" && fileExists(subDir + "\\steam.exe")) {
                        return subDir;
                    }
                }
            }
        }
    }

    return ""; // return nothing if not found
}

std::string findGamePath(const std::string& steamPath, const std::string& gameFolder) {
    std::string commonPath = steamPath + "\\steamapps\\common\\" + gameFolder;

    printf("current common path: %s\n", commonPath);

    if (directoryExists(commonPath)) {
        return commonPath;
    }

    return ""; // return nothing if not found
}

void bypass(HANDLE hProcess) // https://github.com/v3ctra/load-lib-injector
{
    // Restore original NtOpenFile from external process
    //credits: Daniel Krupiñski(pozdro dla ciebie byczku <3)
    if (ntOpenFile) {
        char originalBytes[5];
        memcpy(originalBytes, ntOpenFile, 5);
        WriteProcessMemory(hProcess, ntOpenFile, originalBytes, 5, NULL);
        printf("bypass\n");

    }
}

void Backup(HANDLE hProcess) // https://github.com/v3ctra/load-lib-injector
{
    if (ntOpenFile) {
        //So, when I patching first 5 bytes I need to backup them to 0? (I think)
        char originalBytes[5];
        memcpy(originalBytes, ntOpenFile, 5);
        WriteProcessMemory(hProcess, ntOpenFile, originalBytes, 0, NULL);
        printf("backup!\n");
    }
}
// dll injector by https://github.com/adamhlt/DLL-Injector
int main(const int argc, char* argv[])
{
    const char* lpDLLName = "skeet.dll";
    std::string steamPath = findSteamPath();
    
    // my csgo arguments personally, modify them to suit your own
    // -steam argument is required for the game to run properly
    const char* gameArgs = "-steam -novid -d3d9ex -console -freq 144"
                            "-high +rate 128000 +cl_cmdrate 128 +cl_updaterate 128"
                            " -tickrate 128 +ex_interpratio 1 +cl_interp 0.01"
                            " -noforcemspd -noforcemaccel -noforcemparms -threads 6 -nojoy";
    char lpFullDLLPath[MAX_PATH];
    
    SetConsoleTitleA("Skeet inj version without -insecure");
    printf("make sure that you have skeet.dll in folder with injector\n");
    printf("Waiting for csgo window :)\n");

    if (steamPath.empty()) {
        printf("failed to find steam\n");
        return 1;
    } else {
        printf("steam found: %s", steamPath);
    }
    
    std::string csgoPath = findGamePath(steamPath, "Counter-Strike Global Offensive");

    if (csgoPath.empty()) {
        printf("CS:GO not found\n");
        return 1;
    } else {
        printf("CSGO found");
    }
    
    std::string fullGamePath = csgoPath + "\\csgo.exe";
    const char* gamePath = fullGamePath.c_str();
    HINSTANCE result = ShellExecuteA(NULL, "open", gamePath, gameArgs, NULL, SW_SHOWNORMAL);
    if ((int)result <= 32) {
        MessageBoxA(NULL, "failed to start CS:GO directly.", "Game Start Error", MB_ICONERROR);
        return 1;
    }
    
    HWND window = FindWindowA("Valve001", nullptr);
    DWORD dwProcessID = (DWORD)-1;
    
    while (window == nullptr) 
    {
        window = FindWindowA("Valve001", nullptr); 
        if (window != nullptr)
        {
            GetWindowThreadProcessId(window, &dwProcessID);
            printf("found window. process id: %lu\n", dwProcessID);
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1)); 
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
    bypass(hTargetProcess);
    VirtualAllocEx(hTargetProcess, (LPVOID)0x43310000, 0x2FC000u, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE); // for skeet
    VirtualAllocEx(hTargetProcess, 0, 0x1000u, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE); // for skeet

    LPVOID allocatedMem = VirtualAllocEx(hTargetProcess, NULL, lstrlenA(lpFullDLLPath) + 1, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);


    printf("Memory allocated at 0x%X\n", (UINT)(uintptr_t)allocatedMem);

    const DWORD dwWriteResult = WriteProcessMemory(hTargetProcess, allocatedMem, lpFullDLLPath, lstrlenA(lpFullDLLPath) + 1, NULL);
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

    const HANDLE hThreadCreationResult = CreateRemoteThread(hTargetProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)lpFunctionAddress, allocatedMem, 0, nullptr);
    if (hThreadCreationResult == INVALID_HANDLE_VALUE)
    {
        printf("An error occurred when trying to create the thread in the target process.\n");
        return -1;
    }
    CloseHandle(hTargetProcess);
    printf("DLL Injected!\n");
    Backup(hTargetProcess);
    Sleep(2000);
    return 0;
}
