/* --------------------------------------------------------------------------- */
/*	Used this utility https://github.com/Arty3/SteamPathFinder
	It uses GPLv3 (GNU General Public License Version 3)*/
/* --------------------------------------------------------------------------- */

#pragma once

#include <Windows.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>

#ifdef	AllowGamePathCaching
#undef	AllowGamePathCaching
#endif
#ifdef	GetSteamGamePath
#undef	GetSteamGamePath
#endif
#ifdef	GetSteamLibraryPath
#undef	GetSteamLibraryPath
#endif

#define	AllowGamePathCaching	SteamGamePathFinder::detail::__ALLOW_GAME_PATH_CACHING
#define GetSteamLibraryPath		SteamGamePathFinder::detail::GetSteamPathFromRegistry
#define GetSteamGamePath		SteamGamePathFinder::detail::GetGamePath

namespace SteamGamePathFinder
{
	inline namespace detail
	{
		static bool __ALLOW_GAME_PATH_CACHING = true;

		static constexpr const char* REGEX_PATTERN = R"delim("path"\s*"([^"]+)")delim";
		static constexpr const char	*CACHE_FILE = ".gamepath";
		static constexpr const char	*REGISTRY_KEYS[] = {
			"SOFTWARE\\WOW6432Node\\Valve\\Steam",
			"SOFTWARE\\Valve\\Steam"
		};

		namespace fs = std::filesystem;

		std::optional<std::string> GetSteamPathFromRegistry(void)
		{
			HKEY	hKey;
			LONG	status;
			char	steamPath[MAX_PATH];
			DWORD	steamPathSize = sizeof(steamPath);
		
			for (const char *registryKey : REGISTRY_KEYS)
			{
				if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, registryKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
				{
					status = RegQueryValueExA(hKey, "InstallPath", nullptr, nullptr,
						reinterpret_cast<LPBYTE>(steamPath), &steamPathSize);
		
					RegCloseKey(hKey);
					
					if (status == ERROR_SUCCESS)
						return std::string(steamPath);
				}
			}
			return std::nullopt;
		}

		static std::vector<fs::path> ParseLibraryFolders(const fs::path& baseSteamPath)
		{
			const fs::path libraryFoldersFile = baseSteamPath / "steamapps" / "libraryfolders.vdf";

			std::vector<fs::path> libraryPaths;
		
			libraryPaths.push_back(baseSteamPath);
		
			if (!fs::exists(libraryFoldersFile))
				return libraryPaths;
		
			try
			{
				std::ifstream	file(libraryFoldersFile);
				std::string 	content((std::istreambuf_iterator<char>(file)),
										 std::istreambuf_iterator<char>());
		
				std::string::const_iterator	searchStart(content.cbegin());
				std::regex					pathRegex(REGEX_PATTERN);
				std::smatch					matches;
		
				while (std::regex_search(searchStart, content.cend(), matches, pathRegex))
				{
					if (matches.size() > 1)
					{
						fs::path libraryPath = matches[1].str();
						libraryPath = libraryPath.make_preferred();
						
						if (fs::exists(libraryPath / "steamapps" / "common"))
							libraryPaths.push_back(libraryPath);
					}
					searchStart = matches.suffix().first;
				}
			}
			catch (const std::exception& e)
			{
				std::cerr << "Error reading library folders: " << e.what() << std::endl;
			}
		
			return libraryPaths;
		}

		static std::optional<fs::path> FindGameDirectory(std::string pathName, std::string exeName)
		{
			std::optional<std::string> steamPath = GetSteamPathFromRegistry();

			if (!steamPath)
				return std::nullopt;
		
			std::vector<fs::path> libraryPaths = ParseLibraryFolders(*steamPath);
			
			for (const auto& libraryPath : libraryPaths)
			{
				fs::path commonPath = libraryPath / "steamapps" / "common";
				
				if (!fs::exists(commonPath))
					continue;
		
				try
				{
					for (const auto& entry : fs::directory_iterator(commonPath))
						if (entry.path().filename() == pathName)
							if (fs::exists(entry.path() / fs::path(std::string(exeName))))
								return entry.path();
				}
				catch (const fs::filesystem_error& e)
				{
					std::cerr	<< "Failed to access directory "
								<< commonPath << ": " << e.what() << std::endl;
				}
			}
		
			return std::nullopt;
		}

		static bool WriteCachePath(const fs::path& path)
		{
			if (!__ALLOW_GAME_PATH_CACHING)
				return false;

			try
			{
				std::ofstream cache(CACHE_FILE);
				cache << path.string();
				return true;
			}
			catch (const std::exception& e)
			{
				std::cerr << "Error writing cache: " << e.what() << std::endl;
				return false;
			}
		}

		static std::optional<fs::path> ReadCachePath(void)
		{
			if (!__ALLOW_GAME_PATH_CACHING)
				return std::nullopt;

			try
			{
				std::ifstream cache(CACHE_FILE);
		
				if (!cache)
					return std::nullopt;
		
				std::string path;
				std::getline(cache, path);
				
				return fs::path(path);
			}
			catch (const std::exception& e)
			{
				std::cerr << "Error reading cache: " << e.what() << std::endl;
				return std::nullopt;
			}
		}

		std::optional<fs::path> GetGamePath(std::string pathName, std::string exeName) noexcept
		{
			std::optional<fs::path>	path;

			if (__ALLOW_GAME_PATH_CACHING)
			{
				path = ReadCachePath();
				if (path && fs::exists(*path))
					return path;
			}
		
			path = FindGameDirectory(pathName, exeName);
		
			if (__ALLOW_GAME_PATH_CACHING && path)
				WriteCachePath(*path);
		
			return path;
		}
	}
}
