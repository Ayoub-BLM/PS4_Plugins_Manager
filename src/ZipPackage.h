#pragma once

#include <SDL2/SDL.h>
#include <string>
#include <vector>

namespace ZipPackage {
struct PayloadFile { std::string sourcePath, filename; };
bool install(const std::string& archivePath,
             const std::string& pluginRoot,
             const std::string& relativeInstallPath,
             const std::vector<PayloadFile>& payloadFiles,
             SDL_atomic_t* cancelFlag,
             std::string& error,
             bool overwrite = false);
// Lists what the ZIP would overwrite: for a root install ("."), every ZIP file that already exists under pluginRoot;
// for a sub-folder install, the folder itself if it exists. Does not change anything on disk.
bool listExisting(const std::string& archivePath,
                  const std::string& pluginRoot,
                  const std::string& relativeInstallPath,
                  std::vector<std::string>& existing,
                  std::string& error);
}
