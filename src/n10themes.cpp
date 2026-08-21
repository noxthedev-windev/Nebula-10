#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shlobj.h>
#include "common.hpp"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr const wchar_t* kOfficialRoot = L"C:\\Windows\\NebulaData\\Themes";
// These literal defaults are also part of the documented on-disk contract:
constexpr const wchar_t* kDefaultWallpaperSelection = L"Documents\\Themes\\Wallpapers";
constexpr const wchar_t* kDefaultIconSelection = L"Documents\\Themes\\Icons";

struct Roots {
    fs::path official;
    fs::path selected;
};

struct PackContents {
    std::vector<fs::path> directories;
    std::vector<fs::path> files;
    std::vector<fs::path> wallpaperImages;
};

std::wstring environment(const wchar_t* name) {
    const DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
    if (!required) return L"";
    std::vector<wchar_t> value(required);
    if (!GetEnvironmentVariableW(name, value.data(), required)) return L"";
    return value.data();
}

fs::path absolute_normalized(const fs::path& value) {
    std::error_code error;
    fs::path result = fs::absolute(value, error);
    if (error) result = value;
    return result.lexically_normal();
}

Roots get_roots() {
    std::wstring official = environment(L"NEBULA_THEME_ROOT");
    if (official.empty()) official = kOfficialRoot;

    std::wstring selected = environment(L"NEBULA_THEME_DOCUMENTS");
    if (selected.empty()) {
        wchar_t documents[MAX_PATH]{};
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr,
                                       SHGFP_TYPE_CURRENT, documents))) {
            selected = documents;
        } else {
            selected = environment(L"USERPROFILE");
            if (!selected.empty()) selected += L"\\Documents";
            else selected = L"Documents";
        }
        selected += L"\\Themes";
    }

    return {absolute_normalized(fs::path(official)),
            absolute_normalized(fs::path(selected))};
}

std::wstring lowercase(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

bool path_less(const fs::path& left, const fs::path& right) {
    return lowercase(left.generic_wstring()) < lowercase(right.generic_wstring());
}

bool valid_pack_name(const std::wstring& name) {
    if (name.empty() || name == L"." || name == L"..") return false;
    if (name.back() == L'.' || name.back() == L' ') return false;
    for (wchar_t ch : name) {
        if (ch < 32 || ch == L'/' || ch == L'\\' || ch == L':' ||
            ch == L'<' || ch == L'>' || ch == L'\"' || ch == L'|' ||
            ch == L'?' || ch == L'*') return false;
    }
    return fs::path(name).filename().wstring() == name;
}

bool is_reparse_point(const fs::path& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

bool has_reparse_entry(const fs::path& root, std::wstring& detail) {
    if (!fs::exists(root)) return false;
    if (is_reparse_point(root)) {
        detail = root.wstring();
        return true;
    }
    std::error_code error;
    fs::recursive_directory_iterator iterator(root, error), end;
    if (error) {
        detail = root.wstring();
        return true;
    }
    while (iterator != end) {
        if (is_reparse_point(iterator->path())) {
            detail = iterator->path().wstring();
            return true;
        }
        iterator.increment(error);
        if (error) {
            detail = root.wstring();
            return true;
        }
    }
    return false;
}

bool is_supported_wallpaper(const fs::path& path) {
    const std::wstring extension = lowercase(path.extension().wstring());
    return extension == L".bmp" || extension == L".jpg" ||
           extension == L".jpeg" || extension == L".png";
}

bool inspect_pack(const fs::path& source, PackContents& contents,
                  std::wstring& errorMessage) {
    std::error_code error;
    if (!fs::exists(source, error) || error || !fs::is_directory(source, error) || error) {
        errorMessage = L"Pack directory not found: " + source.wstring();
        return false;
    }

    std::wstring unsafePath;
    if (has_reparse_entry(source, unsafePath)) {
        errorMessage = L"Refused pack containing a reparse point or unreadable entry: " + unsafePath;
        return false;
    }

    fs::recursive_directory_iterator iterator(source, error), end;
    if (error) {
        errorMessage = L"Unable to read pack: " + source.wstring();
        return false;
    }
    while (iterator != end) {
        const fs::directory_entry entry = *iterator;
        const fs::path relative = entry.path().lexically_relative(source);
        if (relative.empty() || relative.is_absolute()) {
            errorMessage = L"Pack entry escaped its source directory: " + entry.path().wstring();
            return false;
        }
        if (entry.is_directory(error) && !error) {
            contents.directories.push_back(relative);
        } else if (entry.is_regular_file(error) && !error) {
            contents.files.push_back(relative);
            if (is_supported_wallpaper(entry.path())) contents.wallpaperImages.push_back(relative);
        } else {
            errorMessage = L"Unsupported pack entry: " + entry.path().wstring();
            return false;
        }
        iterator.increment(error);
        if (error) {
            errorMessage = L"Unable to enumerate pack: " + source.wstring();
            return false;
        }
    }

    std::sort(contents.directories.begin(), contents.directories.end(), path_less);
    std::sort(contents.files.begin(), contents.files.end(), path_less);
    std::sort(contents.wallpaperImages.begin(), contents.wallpaperImages.end(), path_less);
    return true;
}

void print_help() {
    std::wcout
        << L"N10 Themes - install official Nebula10 wallpaper and icon packs\n\n"
        << L"Usage:\n"
        << L"  n10themes --help\n"
        << L"  n10themes roots\n"
        << L"  n10themes list\n"
        << L"  n10themes select-wallpaper <pack> [--dry-run]\n"
        << L"  n10themes select-icons <pack> [--dry-run]\n\n"
        << L"Official root: C:\\Windows\\NebulaData\\Themes\n"
        << L"  Wallpapers: C:\\Windows\\NebulaData\\Themes\\Wallpapers\n"
        << L"  Icons:     C:\\Windows\\NebulaData\\Themes\\Icons\n"
        << L"Selected packs are copied under the current user's Documents\\Themes:\n"
        << L"  " << kDefaultWallpaperSelection << L"\n"
        << L"  " << kDefaultIconSelection << L"\n\n"
        << L"Sandbox overrides: NEBULA_THEME_ROOT and NEBULA_THEME_DOCUMENTS.\n";
}

int invalid_usage(const std::wstring& message) {
    std::wcerr << message << L"\nUse n10themes --help for usage.\n";
    return 2;
}

int show_roots(const Roots& roots) {
    std::wcout << L"N10 Themes roots\n"
               << L"Official root:      " << roots.official.wstring() << L"\n"
               << L"Official wallpapers: " << (roots.official / L"Wallpapers").wstring() << L"\n"
               << L"Official icons:      " << (roots.official / L"Icons").wstring() << L"\n"
               << L"Selected root:       " << roots.selected.wstring() << L"\n"
               << L"Selected wallpapers: " << (roots.selected / L"Wallpapers").wstring() << L"\n"
               << L"Selected icons:      " << (roots.selected / L"Icons").wstring() << L"\n";
    return 0;
}

void list_category(const fs::path& root, const wchar_t* category) {
    const fs::path folder = root / category;
    std::wcout << category << L":\n";
    std::error_code error;
    if (!fs::is_directory(folder, error) || error) {
        std::wcout << L"  (none; folder not found: " << folder.wstring() << L")\n";
        return;
    }

    std::vector<fs::path> packs;
    fs::directory_iterator iterator(folder, error), end;
    while (!error && iterator != end) {
        if (iterator->is_directory(error) && !error && !is_reparse_point(iterator->path()))
            packs.push_back(iterator->path().filename());
        iterator.increment(error);
    }
    std::sort(packs.begin(), packs.end(), path_less);
    if (error) {
        std::wcout << L"  (unable to enumerate folder)\n";
        return;
    }
    if (packs.empty()) std::wcout << L"  (none)\n";
    else for (const fs::path& pack : packs) std::wcout << L"  " << pack.wstring() << L"\n";
}

int list_packs(const Roots& roots) {
    std::wcout << L"N10 Themes official packs\n";
    list_category(roots.official, L"Wallpapers");
    list_category(roots.official, L"Icons");
    return 0;
}

bool copy_pack(const fs::path& source, const fs::path& destination,
               const PackContents& contents, std::wstring& errorMessage) {
    std::error_code error;
    fs::create_directories(destination, error);
    if (error) {
        errorMessage = L"Unable to create destination: " + destination.wstring() +
                       L" (error " + std::to_wstring(error.value()) + L")";
        return false;
    }

    for (const fs::path& relative : contents.directories) {
        fs::create_directories(destination / relative, error);
        if (error) {
            errorMessage = L"Unable to create destination directory: " +
                           (destination / relative).wstring();
            return false;
        }
    }
    for (const fs::path& relative : contents.files) {
        fs::create_directories((destination / relative).parent_path(), error);
        if (error) {
            errorMessage = L"Unable to create destination directory: " +
                           (destination / relative).parent_path().wstring();
            return false;
        }
        const fs::path input = source / relative;
        const fs::path output = destination / relative;
        if (!CopyFileW(input.c_str(), output.c_str(), FALSE)) {
            const DWORD winError = GetLastError();
            errorMessage = L"Unable to copy file: " + input.wstring() +
                           L" (Windows error " + std::to_wstring(winError) + L")";
            return false;
        }
    }
    return true;
}

int select_pack(const Roots& roots, const std::wstring& pack, bool wallpaper,
                bool dryRun) {
    if (!valid_pack_name(pack)) {
        std::wcerr << L"Refused unsafe pack name. Pack must be one direct child name.\n";
        return 2;
    }

    const wchar_t* category = wallpaper ? L"Wallpapers" : L"Icons";
    const fs::path source = absolute_normalized(roots.official / category / pack);
    const fs::path destination = absolute_normalized(roots.selected / category / pack);

    PackContents contents;
    std::wstring message;
    if (!inspect_pack(source, contents, message)) {
        std::wcerr << message << L"\n";
        return 3;
    }
    if (wallpaper && contents.wallpaperImages.empty()) {
        std::wcerr << L"Wallpaper pack has no supported .bmp, .jpg, .jpeg, or .png image.\n";
        return 3;
    }

    std::wstring unsafeDestination;
    if (has_reparse_entry(roots.selected / category, unsafeDestination)) {
        std::wcerr << L"Refused destination containing a reparse point or unreadable entry: "
                   << unsafeDestination << L"\n";
        return 3;
    }

    std::wcout << L"N10 Themes\n"
               << L"Source:      " << source.wstring() << L"\n"
               << L"Destination: " << destination.wstring() << L"\n"
               << L"Files:       " << contents.files.size() << L"\n";

    if (dryRun) {
        if (wallpaper)
            std::wcout << L"Wallpaper:   " << (destination / contents.wallpaperImages.front()).wstring() << L"\n";
        std::wcout << L"DRY-RUN: selection validated. No directories created.\n"
                   << L"No files copied\n";
        return 0;
    }

    // Sandbox overrides are reserved for non-mutating tests and local catalog
    // development. Operations against the protected official store require the
    // installed n10themes binary to match Setup's trusted SHA-256 record.
    if (environment(L"NEBULA_THEME_ROOT").empty() &&
        !nebula::require_nebula_integrity(L"n10themes.exe")) return 8;

    if (!copy_pack(source, destination, contents, message)) {
        std::wcerr << message << L"\n";
        return 5;
    }
    std::wcout << L"Copied pack to: " << destination.wstring() << L"\n";

    if (wallpaper) {
        const fs::path selectedImage = absolute_normalized(destination / contents.wallpaperImages.front());
        // Applying the wallpaper is deliberately sequenced after every file copy succeeds.
        if (!SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0,
                                   const_cast<wchar_t*>(selectedImage.c_str()),
                                   SPIF_UPDATEINIFILE | SPIF_SENDWININICHANGE)) {
            std::wcerr << L"Pack copied, but Windows could not apply wallpaper: "
                       << selectedImage.wstring() << L" (error " << GetLastError() << L")\n";
            return 6;
        }
        std::wcout << L"Applied wallpaper: " << selectedImage.wstring() << L"\n";
    }
    return 0;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc == 2 && (std::wstring(argv[1]) == L"--help" ||
                      std::wstring(argv[1]) == L"-h")) {
        print_help();
        return 0;
    }
    if (argc < 2) return invalid_usage(L"A command is required.");

    const std::wstring command = lowercase(argv[1]);
    const Roots roots = get_roots();
    if (command == L"roots") {
        if (argc != 2) return invalid_usage(L"Usage: n10themes roots");
        return show_roots(roots);
    }
    if (command == L"list") {
        if (argc != 2) return invalid_usage(L"Usage: n10themes list");
        return list_packs(roots);
    }
    if (command == L"select-wallpaper" || command == L"select-icons") {
        if (argc != 3 && argc != 4)
            return invalid_usage(L"Usage: n10themes " + command + L" <pack> [--dry-run]");
        bool dryRun = false;
        if (argc == 4) {
            if (std::wstring(argv[3]) != L"--dry-run")
                return invalid_usage(L"Unknown option: " + std::wstring(argv[3]));
            dryRun = true;
        }
        return select_pack(roots, argv[2], command == L"select-wallpaper", dryRun);
    }
    if (!command.empty() && command[0] == L'-')
        return invalid_usage(L"Unknown option: " + std::wstring(argv[1]));
    return invalid_usage(L"Unknown command: " + std::wstring(argv[1]));
}
