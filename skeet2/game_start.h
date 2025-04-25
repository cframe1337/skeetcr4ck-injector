#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <Windows.h>
#include <Shlwapi.h>

#pragma comment(lib, "Shlwapi.lib")

const char* stringToConstChar(const std::string& str) { return str.c_str(); }

bool directoryExists(const std::string& path) {
    DWORD attr = GetFileAttributesA(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
}

bool fileExists(const std::string& path) {
    return PathFileExistsA(path.c_str()) && !PathIsDirectoryA(path.c_str());
}

std::vector<std::string> getAvailableDrives() {
    std::vector<std::string> drives;
    char buffer[256];
    DWORD length = GetLogicalDriveStringsA(sizeof(buffer), buffer);

    if (length > 0 && length <= sizeof(buffer)) {
        char* drive = buffer;
        while (*drive) {
            if (GetDriveTypeA(drive) == DRIVE_FIXED) { drives.push_back(drive); }
            drive += strlen(drive) + 1;
        }
    }

    return drives;
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
            if (directoryExists(path) && fileExists(path + "\\steam.exe")) { return path; }
        }
    }

    for (const auto& drive : drives) {
        std::vector<std::string> rootDirs = findDirectories(drive);

        for (const auto& dir : rootDirs) {
            if (fileExists(dir + "\\steam.exe")) { return dir; }

            std::string dirName = dir.substr(dir.find_last_of('\\') + 1);

            if (dirName == "Steam" && fileExists(dir + "\\steam.exe")) { return dir; }
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
    if (directoryExists(commonPath)) { return commonPath; }
    return ""; // return nothing if not found
}
