#include "ZipPackage.h"
#include "third_party/miniz/miniz.h"
#include "third_party/miniz/miniz_zip.h"
#include <dirent.h>
#include <set>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <sstream>

namespace ZipPackage {
static const unsigned MAX_FILES = 20000;   // entries (files + folders) per ZIP; was 256, too small for mod packages
static const unsigned long long MAX_EXPANDED = 512ull * 1024ull * 1024ull;
static const unsigned long long MAX_ENTRY = 128ull * 1024ull * 1024ull;

// مكتبة OpenOrbis لا تنفّذ lstat() (تفشل بالخطأ ENOSYS "Function not implemented")، لذلك نستعمل stat() فقط.
// لا توجد روابط رمزية داخل مجلداتنا المؤقتة: مدخلات ZIP من نوع رابط رمزي تُرفض أصلاً عند الفحص.
static int statPath(const std::string& path, struct stat* st) { return stat(path.c_str(), st); }

static bool makeDirs(const std::string& path) {
    if (path.empty() || path[0] != '/') return false;
    std::string current;
    for (size_t i = 0; i < path.size(); ++i) {
        current.push_back(path[i]);
        if (path[i] == '/' || i + 1 == path.size()) {
            if (current.size() <= 1) continue;
            if (mkdir(current.c_str(), 0777) != 0) {
                struct stat st{};
                if (stat(current.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) return false;
            }
        }
    }
    return true;
}

static void removeTree(const std::string& path) {
    struct stat st{};
    if (statPath(path, &st) != 0) return;
    if (!S_ISDIR(st.st_mode)) { unlink(path.c_str()); return; }
    DIR* dir = opendir(path.c_str());
    if (dir) {
        struct dirent* ent;
        while ((ent = readdir(dir)) != nullptr) {
            if (!std::strcmp(ent->d_name, ".") || !std::strcmp(ent->d_name, "..")) continue;
            removeTree(path + "/" + ent->d_name);
        }
        closedir(dir);
    }
    rmdir(path.c_str());
}

static bool ensureDirectoryTracked(const std::string& path, std::vector<std::string>& created,
                                   std::string& error) {
    struct stat st{};
    if (statPath(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) return true;
        error = "ZIP destination path is not a directory: " + path;
        return false;
    }

    // المسار غير موجود (أو لا يمكن فحصه): ننشئ الأب أولاً ثم المجلد نفسه. أي فشل حقيقي يظهر من mkdir.
    const size_t slash = path.rfind('/');
    if (slash == std::string::npos || slash == 0) {
        error = "Invalid ZIP destination path: " + path;
        return false;
    }

    const std::string parent = path.substr(0, slash);
    if (!ensureDirectoryTracked(parent, created, error)) return false;

    if (mkdir(path.c_str(), 0777) != 0) {
        const int createError = errno;
        // قد يكون المجلد أُنشئ بعد الفحص.
        if (statPath(path, &st) == 0 && S_ISDIR(st.st_mode)) return true;
        error = "Cannot create ZIP destination folder '" + path + "': " +
                std::strerror(createError);
        return false;
    }
    created.push_back(path);
    return true;
}

static bool mergeStagedRoot(const std::string& staging, const std::string& destination,
                            const std::set<std::string>& files, const std::set<std::string>& dirs,
                            std::string& error, bool overwrite) {
    std::vector<std::string> createdDirs, movedFiles;
    for (const auto& dir : dirs) {
        struct stat st{};
        const std::string target = destination + "/" + dir;
        if (statPath(target, &st) == 0 && !S_ISDIR(st.st_mode)) { error = "ZIP folder conflicts with an existing file"; return false; }
    }
    for (const auto& file : files) {
        struct stat st{};
        if (!overwrite && statPath(destination + "/" + file, &st) == 0) { error = "ZIP file already exists in installPath: " + file; return false; }
    }
    for (const auto& dir : dirs) {
        if (!ensureDirectoryTracked(destination + "/" + dir, createdDirs, error)) {
            for (auto it = createdDirs.rbegin(); it != createdDirs.rend(); ++it) rmdir(it->c_str());
            return false;
        }
    }
    for (const auto& file : files) {
        const std::string from = staging + "/" + file, to = destination + "/" + file;
        const size_t slash = to.rfind('/');
        if (overwrite) { struct stat old{}; if (statPath(to, &old) == 0 && !S_ISDIR(old.st_mode)) unlink(to.c_str()); }
        if (slash == std::string::npos || !makeDirs(to.substr(0, slash)) || rename(from.c_str(), to.c_str()) != 0) {
            error = "Could not publish ZIP file: " + file;
            for (auto it = movedFiles.rbegin(); it != movedFiles.rend(); ++it) unlink(it->c_str());
            for (auto it = createdDirs.rbegin(); it != createdDirs.rend(); ++it) rmdir(it->c_str());
            return false;
        }
        movedFiles.push_back(to);
    }
    return true;
}

static bool safeRelative(const std::string& path) {
    if (path.empty() || path.size() > 480 || path[0] == '/' || path[0] == '\\') return false;
    size_t start = 0;
    for (size_t i = 0; i <= path.size(); ++i) {
        if (i < path.size() && path[i] != '/') {
            unsigned char c = (unsigned char)path[i];
            if (c < 0x20 || c == '\\' || c == ':' || c == '*') return false;
            continue;
        }
        if (i == start) return false;
        std::string part = path.substr(start, i - start);
        if (part == "." || part == "..") return false;
        start = i + 1;
    }
    return true;
}

static bool safeArchiveName(std::string name, bool directory) {
    while (directory && !name.empty() && name.back() == '/') name.pop_back();
    if (name.empty() || name.size() >= MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE) return false;
    return safeRelative(name);
}

static bool isPluginImage(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    unsigned char magic[4]{};
    size_t n = std::fread(magic, 1, sizeof(magic), f);
    std::fclose(f);
    return n == 4 && ((magic[0] == 0x7f && magic[1] == 'E' && magic[2] == 'L' && magic[3] == 'F') ||
                      (magic[0] == 0x4f && magic[1] == 0x15 && magic[2] == 0x3d && magic[3] == 0x1d));
}

static bool copyFile(const std::string& source, const std::string& destination) {
    FILE* in = std::fopen(source.c_str(), "rb");
    if (!in) return false;
    FILE* out = std::fopen(destination.c_str(), "wb");
    if (!out) { std::fclose(in); return false; }
    unsigned char buffer[32768];
    bool ok = true;
    for (;;) {
        const size_t n = std::fread(buffer, 1, sizeof(buffer), in);
        if (n && std::fwrite(buffer, 1, n, out) != n) { ok = false; break; }
        if (n < sizeof(buffer)) { if (std::ferror(in)) ok = false; break; }
    }
    if (std::fflush(out) != 0) ok = false;
    std::fclose(out); std::fclose(in);
    return ok;
}

bool install(const std::string& archivePath,
             const std::string& pluginRoot,
             const std::string& relativeInstallPath,
             const std::vector<PayloadFile>& payloadFiles,
             SDL_atomic_t* cancelFlag,
             std::string& error,
             bool overwrite) {
    const bool installAtRoot = relativeInstallPath == ".";
    if (!installAtRoot && !safeRelative(relativeInstallPath)) { error = "package installPath must be a safe relative folder"; return false; }
    const std::string finalPath = installAtRoot ? pluginRoot : pluginRoot + "/" + relativeInstallPath;
    const std::string stagingPath = installAtRoot ? archivePath + ".extracting" : finalPath + ".installing";
    const size_t slash = finalPath.rfind('/');
    if (slash == std::string::npos || !makeDirs(slash == 0 ? std::string("/") : finalPath.substr(0, slash))) { error = "Cannot create package destination"; return false; }   // slash == 0: destination is a top-level folder such as /data
    struct stat existing{};
    if (statPath(finalPath, &existing) == 0 && !overwrite && (!installAtRoot || !S_ISDIR(existing.st_mode))) { error = "Package destination already exists"; return false; }

    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, archivePath.c_str(), 0)) { error = "Invalid or unsupported ZIP archive"; return false; }
    const mz_uint count = mz_zip_reader_get_num_files(&zip);
    if (!count || count > MAX_FILES) { mz_zip_reader_end(&zip); error = "ZIP has " + std::to_string(count) + " entries (limit " + std::to_string(MAX_FILES) + ")"; return false; }

    std::set<std::string> names, stagedFiles, stagedDirs;
    unsigned long long expanded = 0;
    bool valid = true;
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat info{};
        char fullName[MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE]{};
        const mz_uint needed = mz_zip_reader_get_filename(&zip, i, nullptr, 0);
        if (!needed || needed >= sizeof(fullName) || !mz_zip_reader_get_filename(&zip, i, fullName, sizeof(fullName)) ||
            !mz_zip_reader_file_stat(&zip, i, &info) || !safeArchiveName(fullName, info.m_is_directory)) { valid = false; break; }
        std::string name(fullName);
        while (info.m_is_directory && !name.empty() && name.back() == '/') name.pop_back();
        if (!names.insert(name).second) { valid = false; break; }
        if (info.m_is_encrypted || !info.m_is_supported) { valid = false; break; }
        if (info.m_is_directory) stagedDirs.insert(name);
        else stagedFiles.insert(name);
        // ZIPs often omit explicit directory entries. Record parents from each
        // validated entry so publication never needs to rescan the staging FS.
        for (size_t slash = name.find('/'); slash != std::string::npos; slash = name.find('/', slash + 1))
            stagedDirs.insert(name.substr(0, slash));
        const unsigned madeBy = info.m_version_made_by >> 8;
        const unsigned unixType = (info.m_external_attr >> 16) & 0170000u;
        if (madeBy == 3 && unixType == 0120000u) { valid = false; break; }
        if (!info.m_is_directory) {
            if (info.m_uncomp_size > MAX_ENTRY || expanded > MAX_EXPANDED - info.m_uncomp_size) { valid = false; break; }
            expanded += info.m_uncomp_size;
        }
    }
    if (!valid) {
        mz_zip_reader_end(&zip);
        error = "ZIP contains an unsafe, duplicate, or unsupported entry";
        return false;
    }
    for (const auto& payload : payloadFiles) {
        if (!safeRelative(payload.filename) || payload.filename.find('/') != std::string::npos) { mz_zip_reader_end(&zip); error = "Unsafe plugin filename"; return false; }
        if (names.find(payload.filename) != names.end()) { mz_zip_reader_end(&zip); error = "ZIP conflicts with a downloaded plugin filename"; return false; }
        stagedFiles.insert(payload.filename);
    }

    removeTree(stagingPath);
    if (!makeDirs(stagingPath)) { mz_zip_reader_end(&zip); error = "Cannot create package staging folder"; return false; }
    for (mz_uint i = 0; i < count; ++i) {
        if (cancelFlag && SDL_AtomicGet(cancelFlag)) { valid = false; error = "Canceled"; break; }
        mz_zip_archive_file_stat info{};
        char fullName[MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE]{};
        if (!mz_zip_reader_file_stat(&zip, i, &info) || !mz_zip_reader_get_filename(&zip, i, fullName, sizeof(fullName))) { valid = false; error = "Could not read ZIP entry"; break; }
        std::string name(fullName);
        while (info.m_is_directory && !name.empty() && name.back() == '/') name.pop_back();
        const std::string output = stagingPath + "/" + name;
        if (info.m_is_directory) {
            if (!makeDirs(output)) { valid = false; error = "Cannot create folder from ZIP"; break; }
            continue;
        }
        const size_t parentSlash = output.rfind('/');
        if (parentSlash == std::string::npos || !makeDirs(output.substr(0, parentSlash)) ||
            !mz_zip_reader_extract_to_file(&zip, i, output.c_str(), 0)) {
            valid = false; error = "Could not extract ZIP entry"; break;
        }
    }
    mz_zip_reader_end(&zip);
    for (const auto& payload : payloadFiles) {
        if (!valid) break;
        if (cancelFlag && SDL_AtomicGet(cancelFlag)) { valid = false; error = "Canceled"; break; }
        const std::string output = stagingPath + "/" + payload.filename;
        if (!copyFile(payload.sourcePath, output) || !isPluginImage(output)) { valid = false; error = "Could not add the downloaded plugin to its package folder"; break; }
    }
    if (valid) {
        if (installAtRoot) valid = mergeStagedRoot(stagingPath, finalPath, stagedFiles, stagedDirs, error, overwrite);
        else if (overwrite && (removeTree(finalPath), false)) { /* old package folder deleted just before publishing the new one */ }
        else if (rename(stagingPath.c_str(), finalPath.c_str()) != 0) { valid = false; error = "Could not publish extracted package"; }
    }
    if (valid && installAtRoot) removeTree(stagingPath);
    if (!valid) removeTree(stagingPath);
    return valid;
}

bool listExisting(const std::string& archivePath, const std::string& pluginRoot,
                  const std::string& relativeInstallPath, std::vector<std::string>& existing,
                  std::string& error) {
    existing.clear();
    const bool installAtRoot = relativeInstallPath == ".";
    if (!installAtRoot && !safeRelative(relativeInstallPath)) { error = "package installPath must be a safe relative folder"; return false; }
    struct stat st{};
    if (!installAtRoot) {
        if (statPath(pluginRoot + "/" + relativeInstallPath, &st) == 0) existing.push_back(relativeInstallPath);
        return true;
    }
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, archivePath.c_str(), 0)) { error = "Invalid or unsupported ZIP archive"; return false; }
    const mz_uint count = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < count && existing.size() < 200; ++i) {
        mz_zip_archive_file_stat info{};
        char fullName[MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE]{};
        if (!mz_zip_reader_file_stat(&zip, i, &info) || info.m_is_directory) continue;
        if (!mz_zip_reader_get_filename(&zip, i, fullName, sizeof(fullName))) continue;
        const std::string name(fullName);
        if (!safeArchiveName(name, false)) continue;
        if (statPath(pluginRoot + "/" + name, &st) == 0) existing.push_back(name);
    }
    mz_zip_reader_end(&zip);
    return true;
}
}
