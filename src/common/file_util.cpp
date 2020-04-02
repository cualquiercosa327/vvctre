// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <limits>
#include <memory>
#include <sstream>
#include <unordered_map>
#include "common/assert.h"
#include "common/common_funcs.h"
#include "common/common_paths.h"
#include "common/file_util.h"
#include "common/logging/log.h"

#ifdef _WIN32
#include <windows.h>
// windows.h needs to be included before other windows headers
#include <direct.h> // getcwd
#include <io.h>
#include <shellapi.h>
#include <shlobj.h> // for SHGetFolderPath
#include <tchar.h>
#include "common/string_util.h"

#ifdef _MSC_VER
// 64 bit offsets for MSVC
#define fseeko _fseeki64
#define ftello _ftelli64
#define fileno _fileno
#endif

// 64 bit offsets for MSVC
#define stat _stat64
#define fstat _fstat64

#else
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <pwd.h>
#include <sys/param.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <sys/stat.h>

#ifndef S_ISDIR
#define S_ISDIR(m) (((m)&S_IFMT) == S_IFDIR)
#endif

// This namespace has various generic functions related to files and paths.
// The code still needs a ton of cleanup.
// REMEMBER: strdup considered harmful!
namespace FileUtil {

// Remove any ending forward slashes from directory paths
// Modifies argument.
static void StripTailDirSlashes(std::string& fname) {
    if (fname.length() <= 1) {
        return;
    }

    std::size_t i = fname.length();
    while (i > 0 && fname[i - 1] == '/') {
        --i;
    }

    fname.resize(i);
}

bool Exists(const std::string& filename) {
    struct stat file_info;

    std::string copy(filename);
    StripTailDirSlashes(copy);

#ifdef _WIN32
    // Windows needs a slash to identify a driver root
    if (copy.size() != 0 && copy.back() == ':') {
        copy += '/';
    }

    int result = _wstat64(Common::UTF8ToUTF16W(copy).c_str(), &file_info);
#else
    int result = stat(copy.c_str(), &file_info);
#endif

    return (result == 0);
}

bool IsDirectory(const std::string& filename) {
    struct stat file_info;

    std::string copy(filename);
    StripTailDirSlashes(copy);

#ifdef _WIN32
    // Windows needs a slash to identify a driver root
    if (copy.size() != 0 && copy.back() == ':') {
        copy += '/';
    }

    int result = _wstat64(Common::UTF8ToUTF16W(copy).c_str(), &file_info);
#else
    int result = stat(copy.c_str(), &file_info);
#endif

    if (result < 0) {
        LOG_DEBUG(Common_Filesystem, "stat failed on {}: {}", filename, GetLastErrorMsg());
        return false;
    }

    return S_ISDIR(file_info.st_mode);
}

bool Delete(const std::string& filename) {
    LOG_TRACE(Common_Filesystem, "file {}", filename);

    // Return true because we care about the file no
    // being there, not the actual delete.
    if (!Exists(filename)) {
        LOG_DEBUG(Common_Filesystem, "{} does not exist", filename);
        return true;
    }

    // We can't delete a directory
    if (IsDirectory(filename)) {
        LOG_ERROR(Common_Filesystem, "Failed: {} is a directory", filename);
        return false;
    }

#ifdef _WIN32
    if (!DeleteFileW(Common::UTF8ToUTF16W(filename).c_str())) {
        LOG_ERROR(Common_Filesystem, "DeleteFile failed on {}: {}", filename, GetLastErrorMsg());
        return false;
    }
#else
    if (unlink(filename.c_str()) == -1) {
        LOG_ERROR(Common_Filesystem, "unlink failed on {}: {}", filename, GetLastErrorMsg());
        return false;
    }
#endif

    return true;
}

bool CreateDir(const std::string& path) {
    LOG_TRACE(Common_Filesystem, "directory {}", path);
#ifdef _WIN32
    if (::CreateDirectoryW(Common::UTF8ToUTF16W(path).c_str(), nullptr))
        return true;
    DWORD error = GetLastError();
    if (error == ERROR_ALREADY_EXISTS) {
        LOG_DEBUG(Common_Filesystem, "CreateDirectory failed on {}: already exists", path);
        return true;
    }
    LOG_ERROR(Common_Filesystem, "CreateDirectory failed on {}: {}", path, error);
    return false;
#else
    if (mkdir(path.c_str(), 0755) == 0)
        return true;

    int err = errno;

    if (err == EEXIST) {
        LOG_DEBUG(Common_Filesystem, "mkdir failed on {}: already exists", path);
        return true;
    }

    LOG_ERROR(Common_Filesystem, "mkdir failed on {}: {}", path, strerror(err));
    return false;
#endif
}

bool CreateFullPath(const std::string& fullPath) {
    int panicCounter = 100;
    LOG_TRACE(Common_Filesystem, "path {}", fullPath);

    if (FileUtil::Exists(fullPath)) {
        LOG_DEBUG(Common_Filesystem, "path exists {}", fullPath);
        return true;
    }

    std::size_t position = 0;
    while (true) {
        // Find next sub path
        position = fullPath.find('/', position);

        // We're done, yay!
        if (position == fullPath.npos) {
            return true;
        }

        // Include the '/' so the first call is CreateDir("/") rather than CreateDir("")
        std::string const subPath(fullPath.substr(0, position + 1));
        if (!FileUtil::IsDirectory(subPath) && !FileUtil::CreateDir(subPath)) {
            LOG_ERROR(Common, "CreateFullPath: directory creation failed");
            return false;
        }

        // A safety check
        panicCounter--;
        if (panicCounter <= 0) {
            LOG_ERROR(Common, "CreateFullPath: directory structure is too deep");
            return false;
        }
        position++;
    }
}

bool DeleteDir(const std::string& filename) {
    LOG_TRACE(Common_Filesystem, "directory {}", filename);

    // check if a directory
    if (!FileUtil::IsDirectory(filename)) {
        LOG_ERROR(Common_Filesystem, "Not a directory {}", filename);
        return false;
    }

#ifdef _WIN32
    if (::RemoveDirectoryW(Common::UTF8ToUTF16W(filename).c_str()))
        return true;
#else
    if (rmdir(filename.c_str()) == 0)
        return true;
#endif
    LOG_ERROR(Common_Filesystem, "failed {}: {}", filename, GetLastErrorMsg());

    return false;
}

bool Rename(const std::string& srcFilename, const std::string& destFilename) {
    LOG_TRACE(Common_Filesystem, "{} --> {}", srcFilename, destFilename);
#ifdef _WIN32
    if (_wrename(Common::UTF8ToUTF16W(srcFilename).c_str(),
                 Common::UTF8ToUTF16W(destFilename).c_str()) == 0)
        return true;
#else
    if (rename(srcFilename.c_str(), destFilename.c_str()) == 0)
        return true;
#endif
    LOG_ERROR(Common_Filesystem, "failed {} --> {}: {}", srcFilename, destFilename,
              GetLastErrorMsg());
    return false;
}

bool Copy(const std::string& srcFilename, const std::string& destFilename) {
    LOG_TRACE(Common_Filesystem, "{} --> {}", srcFilename, destFilename);
#ifdef _WIN32
    if (CopyFileW(Common::UTF8ToUTF16W(srcFilename).c_str(),
                  Common::UTF8ToUTF16W(destFilename).c_str(), FALSE))
        return true;

    LOG_ERROR(Common_Filesystem, "failed {} --> {}: {}", srcFilename, destFilename,
              GetLastErrorMsg());
    return false;
#else
    using CFilePointer = std::unique_ptr<FILE, decltype(&std::fclose)>;

    // Open input file
    CFilePointer input{fopen(srcFilename.c_str(), "rb"), std::fclose};
    if (!input) {
        LOG_ERROR(Common_Filesystem, "opening input failed {} --> {}: {}", srcFilename,
                  destFilename, GetLastErrorMsg());
        return false;
    }

    // open output file
    CFilePointer output{fopen(destFilename.c_str(), "wb"), std::fclose};
    if (!output) {
        LOG_ERROR(Common_Filesystem, "opening output failed {} --> {}: {}", srcFilename,
                  destFilename, GetLastErrorMsg());
        return false;
    }

    // copy loop
    std::array<char, 1024> buffer;
    while (!feof(input.get())) {
        // read input
        std::size_t rnum = fread(buffer.data(), sizeof(char), buffer.size(), input.get());
        if (rnum != buffer.size()) {
            if (ferror(input.get()) != 0) {
                LOG_ERROR(Common_Filesystem, "failed reading from source, {} --> {}: {}",
                          srcFilename, destFilename, GetLastErrorMsg());
                return false;
            }
        }

        // write output
        std::size_t wnum = fwrite(buffer.data(), sizeof(char), rnum, output.get());
        if (wnum != rnum) {
            LOG_ERROR(Common_Filesystem, "failed writing to output, {} --> {}: {}", srcFilename,
                      destFilename, GetLastErrorMsg());
            return false;
        }
    }

    return true;
#endif
}

u64 GetSize(const std::string& filename) {
    if (!Exists(filename)) {
        LOG_ERROR(Common_Filesystem, "failed {}: No such file", filename);
        return 0;
    }

    if (IsDirectory(filename)) {
        LOG_ERROR(Common_Filesystem, "failed {}: is a directory", filename);
        return 0;
    }

    struct stat buf;
#ifdef _WIN32
    if (_wstat64(Common::UTF8ToUTF16W(filename).c_str(), &buf) == 0)
#else
    if (stat(filename.c_str(), &buf) == 0)
#endif
    {
        LOG_TRACE(Common_Filesystem, "{}: {}", filename, buf.st_size);
        return buf.st_size;
    }

    LOG_ERROR(Common_Filesystem, "Stat failed {}: {}", filename, GetLastErrorMsg());
    return 0;
}

u64 GetSize(const int fd) {
    struct stat buf;
    if (fstat(fd, &buf) != 0) {
        LOG_ERROR(Common_Filesystem, "GetSize: stat failed {}: {}", fd, GetLastErrorMsg());
        return 0;
    }
    return buf.st_size;
}

u64 GetSize(FILE* f) {
    // can't use off_t here because it can be 32-bit
    u64 pos = ftello(f);
    if (fseeko(f, 0, SEEK_END) != 0) {
        LOG_ERROR(Common_Filesystem, "GetSize: seek failed {}: {}", fmt::ptr(f), GetLastErrorMsg());
        return 0;
    }
    u64 size = ftello(f);
    if ((size != pos) && (fseeko(f, pos, SEEK_SET) != 0)) {
        LOG_ERROR(Common_Filesystem, "GetSize: seek failed {}: {}", fmt::ptr(f), GetLastErrorMsg());
        return 0;
    }
    return size;
}

bool CreateEmptyFile(const std::string& filename) {
    LOG_TRACE(Common_Filesystem, "{}", filename);

    if (!FileUtil::IOFile(filename, "wb").IsOpen()) {
        LOG_ERROR(Common_Filesystem, "failed {}: {}", filename, GetLastErrorMsg());
        return false;
    }

    return true;
}

bool ForeachDirectoryEntry(u64* num_entries_out, const std::string& directory,
                           DirectoryEntryCallable callback) {
    LOG_TRACE(Common_Filesystem, "directory {}", directory);

    // How many files + directories we found
    u64 found_entries = 0;

    // Save the status of callback function
    bool callback_error = false;

#ifdef _WIN32
    // Find the first file in the directory.
    WIN32_FIND_DATAW ffd;

    HANDLE handle_find = FindFirstFileW(Common::UTF8ToUTF16W(directory + "\\*").c_str(), &ffd);
    if (handle_find == INVALID_HANDLE_VALUE) {
        FindClose(handle_find);
        return false;
    }
    // windows loop
    do {
        const std::string virtual_name(Common::UTF16ToUTF8(ffd.cFileName));
#else
    DIR* dirp = opendir(directory.c_str());
    if (!dirp)
        return false;

    // non windows loop
    while (struct dirent* result = readdir(dirp)) {
        const std::string virtual_name(result->d_name);
#endif

        if (virtual_name == "." || virtual_name == "..")
            continue;

        u64 ret_entries = 0;
        if (!callback(&ret_entries, directory, virtual_name)) {
            callback_error = true;
            break;
        }
        found_entries += ret_entries;

#ifdef _WIN32
    } while (FindNextFileW(handle_find, &ffd) != 0);
    FindClose(handle_find);
#else
    }
    closedir(dirp);
#endif

    if (callback_error)
        return false;

    // num_entries_out is allowed to be specified nullptr, in which case we shouldn't try to set it
    if (num_entries_out != nullptr)
        *num_entries_out = found_entries;
    return true;
}

u64 ScanDirectoryTree(const std::string& directory, FSTEntry& parent_entry,
                      unsigned int recursion) {
    const auto callback = [recursion, &parent_entry](u64* num_entries_out,
                                                     const std::string& directory,
                                                     const std::string& virtual_name) -> bool {
        FSTEntry entry;
        entry.virtualName = virtual_name;
        entry.physicalName = directory + "/" + virtual_name;

        if (IsDirectory(entry.physicalName)) {
            entry.isDirectory = true;
            // is a directory, lets go inside if we didn't recurse to often
            if (recursion > 0) {
                entry.size = ScanDirectoryTree(entry.physicalName, entry, recursion - 1);
                *num_entries_out += entry.size;
            } else {
                entry.size = 0;
            }
        } else { // is a file
            entry.isDirectory = false;
            entry.size = GetSize(entry.physicalName);
        }
        (*num_entries_out)++;

        // Push into the tree
        parent_entry.children.push_back(std::move(entry));
        return true;
    };

    u64 num_entries;
    return ForeachDirectoryEntry(&num_entries, directory, callback) ? num_entries : 0;
}

void GetAllFilesFromNestedEntries(FSTEntry& directory, std::vector<FSTEntry>& output) {
    std::vector<FSTEntry> files;
    for (auto& entry : directory.children) {
        if (entry.isDirectory) {
            GetAllFilesFromNestedEntries(entry, output);
        } else {
            output.push_back(entry);
        }
    }
}

bool DeleteDirRecursively(const std::string& directory, unsigned int recursion) {
    const auto callback = [recursion](u64* num_entries_out, const std::string& directory,
                                      const std::string& virtual_name) -> bool {
        std::string new_path = directory + '/' + virtual_name;

        if (IsDirectory(new_path)) {
            if (recursion == 0)
                return false;
            return DeleteDirRecursively(new_path, recursion - 1);
        }
        return Delete(new_path);
    };

    if (!ForeachDirectoryEntry(nullptr, directory, callback))
        return false;

    // Delete the outermost directory
    FileUtil::DeleteDir(directory);
    return true;
}

void CopyDir(const std::string& source_path, const std::string& dest_path) {
#ifndef _WIN32
    if (source_path == dest_path)
        return;
    if (!FileUtil::Exists(source_path))
        return;
    if (!FileUtil::Exists(dest_path))
        FileUtil::CreateFullPath(dest_path);

    DIR* dirp = opendir(source_path.c_str());
    if (!dirp)
        return;

    while (struct dirent* result = readdir(dirp)) {
        const std::string virtualName(result->d_name);
        // check for "." and ".."
        if (((virtualName[0] == '.') && (virtualName[1] == '\0')) ||
            ((virtualName[0] == '.') && (virtualName[1] == '.') && (virtualName[2] == '\0')))
            continue;

        std::string source, dest;
        source = source_path + virtualName;
        dest = dest_path + virtualName;
        if (IsDirectory(source)) {
            source += '/';
            dest += '/';
            if (!FileUtil::Exists(dest))
                FileUtil::CreateFullPath(dest);
            CopyDir(source, dest);
        } else if (!FileUtil::Exists(dest))
            FileUtil::Copy(source, dest);
    }
    closedir(dirp);
#endif
}

std::optional<std::string> GetCurrentDir() {
    // Get the current working directory (getcwd uses malloc)
#ifdef _WIN32
    wchar_t* dir = _wgetcwd(nullptr, 0);
    if (!dir) {
#else
    char* dir = getcwd(nullptr, 0);
    if (!dir) {
#endif
        LOG_ERROR(Common_Filesystem, "GetCurrentDirectory failed: {}", GetLastErrorMsg());
        return {};
    }
#ifdef _WIN32
    std::string strDir = Common::UTF16ToUTF8(dir);
#else
    std::string strDir = dir;
#endif
    free(dir);
    return strDir;
}

bool SetCurrentDir(const std::string& directory) {
#ifdef _WIN32
    return _wchdir(Common::UTF8ToUTF16W(directory).c_str()) == 0;
#else
    return chdir(directory.c_str()) == 0;
#endif
}

#ifdef _WIN32
const std::string& GetExeDirectory() {
    static std::string exe_path;
    if (exe_path.empty()) {
        wchar_t wchar_exe_path[2048];
        GetModuleFileNameW(nullptr, wchar_exe_path, 2048);
        exe_path = Common::UTF16ToUTF8(wchar_exe_path);
        exe_path = exe_path.substr(0, exe_path.find_last_of('\\'));
    }
    return exe_path;
}

std::string AppDataRoamingDirectory() {
    PWSTR pw_local_path = nullptr;
    // Only supported by Windows Vista or later
    SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &pw_local_path);
    std::string local_path = Common::UTF16ToUTF8(pw_local_path);
    CoTaskMemFree(pw_local_path);
    return local_path;
}
#else
/**
 * @return The user’s home directory on POSIX systems
 */
static const std::string& GetHomeDirectory() {
    static std::string home_path;
    if (home_path.empty()) {
        const char* envvar = getenv("HOME");
        if (envvar) {
            home_path = envvar;
        } else {
            auto pw = getpwuid(getuid());
            ASSERT_MSG(pw,
                       "$HOME isn’t defined, and the current user can’t be found in /etc/passwd.");
            home_path = pw->pw_dir;
        }
    }
    return home_path;
}

/**
 * Follows the XDG Base Directory Specification to get a directory path
 * @param envvar The XDG environment variable to get the value from
 * @return The directory path
 * @sa http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html
 */
static const std::string GetUserDirectory(const std::string& envvar) {
    const char* directory = getenv(envvar.c_str());

    std::string user_dir;
    if (directory) {
        user_dir = directory;
    } else {
        std::string subdirectory;

        if (envvar == "XDG_DATA_HOME") {
            subdirectory = "/.local/share";
        } else {
            UNIMPLEMENTED_MSG("XDG variable {}", envvar);
        }

        user_dir = GetHomeDirectory() + subdirectory;
    }

    ASSERT_MSG(!user_dir.empty(), "User directory {} musn’t be empty.", envvar);
    ASSERT_MSG(user_dir[0] == '/', "User directory {} must be absolute.", envvar);

    return user_dir;
}
#endif

namespace {
std::unordered_map<UserPath, std::string> g_paths;
}

void SetUserPath(const std::string& path) {
    std::string& user_path = g_paths[UserPath::UserDir];

    if (!path.empty() && CreateFullPath(path)) {
        LOG_INFO(Common_Filesystem, "Using {} as the user directory", path);
        user_path = path;
    } else {
#ifdef _WIN32
        user_path = GetExeDirectory() + "/" USERDATA_DIR "/";
        if (!FileUtil::IsDirectory(user_path)) {
            user_path = AppDataRoamingDirectory() + "/" EMU_DATA_DIR "/";
        } else {
            LOG_INFO(Common_Filesystem, "Using the local user directory");
        }

#else
        if (FileUtil::Exists(ROOT_DIR "/" USERDATA_DIR)) {
            user_path = ROOT_DIR "/" USERDATA_DIR "/";
        } else {
            std::string data_dir = GetUserDirectory("XDG_DATA_HOME");

            user_path = data_dir + "/" EMU_DATA_DIR "/";
        }
#endif
    }

    g_paths[UserPath::SDMCDir] = user_path + SDMC_DIR "/";
    g_paths[UserPath::NANDDir] = user_path + NAND_DIR "/";
    g_paths[UserPath::SysDataDir] = user_path + SYSDATA_DIR "/";
    g_paths[UserPath::LogDir] = user_path + LOG_DIR "/";
    g_paths[UserPath::CheatsDir] = user_path + CHEATS_DIR "/";
    g_paths[UserPath::ShaderDir] = user_path + SHADER_DIR "/";
    g_paths[UserPath::DumpDir] = user_path + DUMP_DIR "/";
    g_paths[UserPath::LoadDir] = user_path + LOAD_DIR "/";
}

const std::string& GetUserPath(UserPath path) {
    // Set up all paths and files on the first run
    if (g_paths.empty())
        SetUserPath();
    return g_paths[path];
}
std::size_t WriteStringToFile(bool text_file, const std::string& filename, std::string_view str) {
    return IOFile(filename, text_file ? "w" : "wb").WriteString(str);
}

std::size_t ReadFileToString(bool text_file, const std::string& filename, std::string& str) {
    IOFile file(filename, text_file ? "r" : "rb");

    if (!file.IsOpen())
        return 0;

    str.resize(static_cast<u32>(file.GetSize()));
    return file.ReadArray(&str[0], str.size());
}

void SplitFilename83(const std::string& filename, std::array<char, 9>& short_name,
                     std::array<char, 4>& extension) {
    const std::string forbidden_characters = ".\"/\\[]:;=, ";

    // On a FAT32 partition, 8.3 names are stored as a 11 bytes array, filled with spaces.
    short_name = {{' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', '\0'}};
    extension = {{' ', ' ', ' ', '\0'}};

    std::string::size_type point = filename.rfind('.');
    if (point == filename.size() - 1)
        point = filename.rfind('.', point);

    // Get short name.
    int j = 0;
    for (char letter : filename.substr(0, point)) {
        if (forbidden_characters.find(letter, 0) != std::string::npos)
            continue;
        if (j == 8) {
            // TODO(Link Mauve): also do that for filenames containing a space.
            // TODO(Link Mauve): handle multiple files having the same short name.
            short_name[6] = '~';
            short_name[7] = '1';
            break;
        }
        short_name[j++] = toupper(letter);
    }

    // Get extension.
    if (point != std::string::npos) {
        j = 0;
        for (char letter : filename.substr(point + 1, 3))
            extension[j++] = toupper(letter);
    }
}

std::vector<std::string> SplitPathComponents(std::string_view filename) {
    std::string copy(filename);
    std::replace(copy.begin(), copy.end(), '\\', '/');
    std::vector<std::string> out;

    std::stringstream stream(copy);
    std::string item;
    while (std::getline(stream, item, '/')) {
        out.push_back(std::move(item));
    }

    return out;
}

std::string_view GetParentPath(std::string_view path) {
    const auto name_bck_index = path.rfind('\\');
    const auto name_fwd_index = path.rfind('/');
    std::size_t name_index;

    if (name_bck_index == std::string_view::npos || name_fwd_index == std::string_view::npos) {
        name_index = std::min(name_bck_index, name_fwd_index);
    } else {
        name_index = std::max(name_bck_index, name_fwd_index);
    }

    return path.substr(0, name_index);
}

std::string_view GetPathWithoutTop(std::string_view path) {
    if (path.empty()) {
        return path;
    }

    while (path[0] == '\\' || path[0] == '/') {
        path.remove_prefix(1);
        if (path.empty()) {
            return path;
        }
    }

    const auto name_bck_index = path.find('\\');
    const auto name_fwd_index = path.find('/');
    return path.substr(std::min(name_bck_index, name_fwd_index) + 1);
}

std::string_view GetFilename(std::string_view path) {
    const auto name_index = path.find_last_of("\\/");

    if (name_index == std::string_view::npos) {
        return {};
    }

    return path.substr(name_index + 1);
}

std::string_view GetExtensionFromFilename(std::string_view name) {
    const std::size_t index = name.rfind('.');

    if (index == std::string_view::npos) {
        return {};
    }

    return name.substr(index + 1);
}

std::string_view RemoveTrailingSlash(std::string_view path) {
    if (path.empty()) {
        return path;
    }

    if (path.back() == '\\' || path.back() == '/') {
        path.remove_suffix(1);
        return path;
    }

    return path;
}

std::string SanitizePath(std::string_view path_, DirectorySeparator directory_separator) {
    std::string path(path_);
    char type1 = directory_separator == DirectorySeparator::BackwardSlash ? '/' : '\\';
    char type2 = directory_separator == DirectorySeparator::BackwardSlash ? '\\' : '/';

    if (directory_separator == DirectorySeparator::PlatformDefault) {
#ifdef _WIN32
        type1 = '/';
        type2 = '\\';
#endif
    }

    std::replace(path.begin(), path.end(), type1, type2);
    path.erase(std::unique(path.begin(), path.end(),
                           [type2](char c1, char c2) { return c1 == type2 && c2 == type2; }),
               path.end());
    return std::string(RemoveTrailingSlash(path));
}

IOFile::IOFile() {}

IOFile::IOFile(const std::string& filename, const char openmode[], int flags) {
    Open(filename, openmode, flags);
}

IOFile::~IOFile() {
    Close();
}

IOFile::IOFile(IOFile&& other) noexcept {
    Swap(other);
}

IOFile& IOFile::operator=(IOFile&& other) noexcept {
    Swap(other);
    return *this;
}

void IOFile::Swap(IOFile& other) noexcept {
    std::swap(m_file, other.m_file);
    std::swap(m_good, other.m_good);
}

bool IOFile::Open(const std::string& filename, const char openmode[], int flags) {
    Close();
#ifdef _WIN32
    if (flags != 0) {
        m_file = _wfsopen(Common::UTF8ToUTF16W(filename).c_str(),
                          Common::UTF8ToUTF16W(openmode).c_str(), flags);
        m_good = m_file != nullptr;
    } else {
        m_good = _wfopen_s(&m_file, Common::UTF8ToUTF16W(filename).c_str(),
                           Common::UTF8ToUTF16W(openmode).c_str()) == 0;
    }
#else
    m_file = std::fopen(filename.c_str(), openmode);
    m_good = m_file != nullptr;
#endif

    return m_good;
}

bool IOFile::Close() {
    if (!IsOpen() || 0 != std::fclose(m_file))
        m_good = false;

    m_file = nullptr;
    return m_good;
}

u64 IOFile::GetSize() const {
    if (IsOpen())
        return FileUtil::GetSize(m_file);

    return 0;
}

bool IOFile::Seek(s64 off, int origin) {
    if (!IsOpen() || 0 != fseeko(m_file, off, origin))
        m_good = false;

    return m_good;
}

u64 IOFile::Tell() const {
    if (IsOpen())
        return ftello(m_file);

    return std::numeric_limits<u64>::max();
}

bool IOFile::Flush() {
    if (!IsOpen() || 0 != std::fflush(m_file))
        m_good = false;

    return m_good;
}

bool IOFile::Resize(u64 size) {
    if (!IsOpen() || 0 !=
#ifdef _WIN32
                         // ector: _chsize sucks, not 64-bit safe
                         // F|RES: changed to _chsize_s. i think it is 64-bit safe
                         _chsize_s(_fileno(m_file), size)
#else
                         // TODO: handle 64bit and growing
                         ftruncate(fileno(m_file), size)
#endif
    )
        m_good = false;

    return m_good;
}

} // namespace FileUtil
