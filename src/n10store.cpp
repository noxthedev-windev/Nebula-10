#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include "common.hpp"
#include <shlobj.h>
#include <shobjidl.h>
#include <conio.h>
#include <algorithm>
#include <cwctype>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool create_store_shortcut(const std::wstring& link,const std::wstring& target,const std::wstring& icon){
    IShellLinkW* shell{};if(FAILED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_IShellLinkW,reinterpret_cast<void**>(&shell))))return false;shell->SetPath(target.c_str());shell->SetWorkingDirectory(nebula::exe_dir().c_str());shell->SetDescription(L"Nebula Store");shell->SetIconLocation(icon.c_str(),0);IPersistFile* persist{};bool ok=SUCCEEDED(shell->QueryInterface(IID_IPersistFile,reinterpret_cast<void**>(&persist)))&&SUCCEEDED(persist->Save(link.c_str(),TRUE));if(persist)persist->Release();shell->Release();return ok;
}
void ensure_store_shortcuts(){
    std::wstring installed=nebula::reg_string(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Nebula10\\Identity",L"InstallRoot");if(installed.empty()||_wcsicmp(installed.c_str(),nebula::exe_dir().c_str())!=0)return;
    HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);bool uninitialize=SUCCEEDED(initialized);if(FAILED(initialized)&&initialized!=RPC_E_CHANGED_MODE)return;std::wstring target=nebula::exe_dir()+L"\\N10Store.exe",icon=nebula::exe_dir()+L"\\Assets\\Branding\\N10Store.ico";PWSTR desktop{},programs{};
    if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop,0,nullptr,&desktop))){std::wstring link=std::wstring(desktop)+L"\\Nebula Store.lnk";if(GetFileAttributesW(link.c_str())==INVALID_FILE_ATTRIBUTES)create_store_shortcut(link,target,icon);CoTaskMemFree(desktop);}
    if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Programs,0,nullptr,&programs))){std::wstring folder=std::wstring(programs)+L"\\Nebula10";CreateDirectoryW(folder.c_str(),nullptr);std::wstring link=folder+L"\\Nebula Store.lnk";if(GetFileAttributesW(link.c_str())==INVALID_FILE_ATTRIBUTES)create_store_shortcut(link,target,icon);CoTaskMemFree(programs);}
    if(uninitialize)CoUninitialize();
}

struct Package {
    const wchar_t* slug;
    const wchar_t* name;
    const wchar_t* id;
    const wchar_t* category;
    const wchar_t* description;
};

// Deliberately curated and compiled in: package identifiers are never accepted from input.
static const Package kCatalog[] = {
    {L"chrome", L"Google Chrome", L"Google.Chrome", L"Browsers", L"Google's fast, widely compatible web browser."},
    {L"firefox", L"Mozilla Firefox", L"Mozilla.Firefox", L"Browsers", L"Open-source browser with strong privacy controls."},
    {L"brave", L"Brave", L"Brave.Brave", L"Browsers", L"Privacy-focused Chromium browser with built-in blocking."},
    {L"vivaldi", L"Vivaldi", L"Vivaldi.Vivaldi", L"Browsers", L"Highly customizable Chromium-based browser."},
    {L"opera", L"Opera", L"Opera.Opera", L"Browsers", L"Feature-rich Chromium browser."},
    {L"edge", L"Microsoft Edge", L"Microsoft.Edge", L"Browsers", L"Microsoft's Chromium-based browser."},

    {L"7zip", L"7-Zip", L"7zip.7zip", L"Utilities", L"Lightweight file archiver with broad format support."},
    {L"peazip", L"PeaZip", L"Giorgiotani.Peazip", L"Utilities", L"Open-source archive manager."},
    {L"everything", L"Everything", L"voidtools.Everything", L"Utilities", L"Instant filename search for Windows."},
    {L"powertoys", L"Microsoft PowerToys", L"Microsoft.PowerToys", L"Utilities", L"Productivity utilities for Windows power users."},
    {L"rufus", L"Rufus", L"Rufus.Rufus", L"Utilities", L"Create bootable USB drives."},
    {L"winscp", L"WinSCP", L"WinSCP.WinSCP", L"Utilities", L"Graphical SFTP, FTP, and SCP client."},

    {L"vscode", L"Visual Studio Code", L"Microsoft.VisualStudioCode", L"Development", L"Extensible source-code editor."},
    {L"git", L"Git", L"Git.Git", L"Development", L"Distributed version-control tools for Windows."},
    {L"github-desktop", L"GitHub Desktop", L"GitHub.GitHubDesktop", L"Development", L"Graphical Git and GitHub client."},
    {L"python", L"Python 3.12", L"Python.Python.3.12", L"Development", L"Python programming language and standard tools."},
    {L"nodejs", L"Node.js LTS", L"OpenJS.NodeJS.LTS", L"Development", L"Long-term-support JavaScript runtime."},
    {L"cmake", L"CMake", L"Kitware.CMake", L"Development", L"Cross-platform build-system generator."},
    {L"notepadpp", L"Notepad++", L"Notepad++.Notepad++", L"Development", L"Fast source-code and text editor."},

    {L"vlc", L"VLC media player", L"VideoLAN.VLC", L"Media", L"Open-source multimedia player."},
    {L"obs", L"OBS Studio", L"OBSProject.OBSStudio", L"Media", L"Video recording and live-streaming studio."},
    {L"audacity", L"Audacity", L"Audacity.Audacity", L"Media", L"Multi-track audio editor and recorder."},
    {L"handbrake", L"HandBrake", L"HandBrake.HandBrake", L"Media", L"Open-source video transcoder."},
    {L"spotify", L"Spotify", L"Spotify.Spotify", L"Media", L"Music and podcast streaming client."},
    {L"gimp", L"GIMP", L"GIMP.GIMP.3", L"Media", L"Open-source image editor."},

    {L"steam", L"Steam", L"Valve.Steam", L"Gaming", L"PC game storefront, library, and community."},
    {L"epic-games", L"Epic Games Launcher", L"EpicGames.EpicGamesLauncher", L"Gaming", L"Epic game storefront and library."},
    {L"gog-galaxy", L"GOG GALAXY", L"GOG.Galaxy", L"Gaming", L"GOG game library and launcher."},
    {L"discord", L"Discord", L"Discord.Discord", L"Gaming", L"Voice, video, and text communities."},
    {L"playnite", L"Playnite", L"Playnite.Playnite", L"Gaming", L"Open-source unified game-library manager."},
    {L"prismlauncher", L"Prism Launcher", L"PrismLauncher.PrismLauncher", L"Gaming", L"Open-source Minecraft instance manager."},

    {L"teams", L"Microsoft Teams", L"Microsoft.Teams", L"Communication", L"Microsoft work chat and meetings client."},
    {L"zoom", L"Zoom Workplace", L"Zoom.Zoom", L"Communication", L"Video meetings and collaboration."},
    {L"slack", L"Slack", L"SlackTechnologies.Slack", L"Communication", L"Team messaging and collaboration client."},
    {L"telegram", L"Telegram Desktop", L"Telegram.TelegramDesktop", L"Communication", L"Cloud messaging desktop client."},
    {L"signal", L"Signal", L"OpenWhisperSystems.Signal", L"Communication", L"End-to-end encrypted messaging."},
    {L"thunderbird", L"Mozilla Thunderbird", L"Mozilla.Thunderbird", L"Communication", L"Open-source email and calendar client."},

    {L"bitwarden", L"Bitwarden", L"Bitwarden.Bitwarden", L"Security", L"Open-source password manager."},
    {L"keepassxc", L"KeePassXC", L"KeePassXCTeam.KeePassXC", L"Security", L"Offline cross-platform password manager."},
    {L"malwarebytes", L"Malwarebytes", L"Malwarebytes.Malwarebytes", L"Security", L"Malware detection and removal utility."},
    {L"veracrypt", L"VeraCrypt", L"IDRIX.VeraCrypt", L"Security", L"Open-source disk and volume encryption."},
    {L"gpg4win", L"Gpg4win", L"GnuPG.Gpg4win", L"Security", L"OpenPGP encryption and signing suite."},

    {L"terminal", L"Windows Terminal", L"Microsoft.WindowsTerminal", L"System Tools", L"Modern tabbed terminal from Microsoft."},
    {L"process-explorer", L"Process Explorer", L"Microsoft.Sysinternals.ProcessExplorer", L"System Tools", L"Detailed process and handle inspector."},
    {L"autoruns", L"Autoruns", L"Microsoft.Sysinternals.Autoruns", L"System Tools", L"Inspect programs configured to start automatically."},
    {L"wireshark", L"Wireshark", L"WiresharkFoundation.Wireshark", L"System Tools", L"Network protocol analyzer."},
    {L"windirstat", L"WinDirStat", L"WinDirStat.WinDirStat", L"System Tools", L"Visual disk-usage analyzer."},
    {L"hwinfo", L"HWiNFO", L"REALiX.HWiNFO", L"System Tools", L"Detailed hardware information and monitoring."},
    {L"cpu-z", L"CPU-Z", L"CPUID.CPU-Z", L"System Tools", L"CPU, mainboard, and memory information."}
};

constexpr size_t kCatalogSize = sizeof(kCatalog) / sizeof(kCatalog[0]);
static const wchar_t* kCategories[] = {L"Browsers", L"Utilities", L"Development", L"Media", L"Gaming", L"Communication", L"Security", L"System Tools"};

const wchar_t* choco_id(const Package& package) {
    struct Mapping { const wchar_t* slug; const wchar_t* id; };
    static const Mapping ids[] = {
        {L"chrome",L"googlechrome"},{L"firefox",L"firefox"},{L"brave",L"brave"},{L"vivaldi",L"vivaldi"},{L"opera",L"opera"},{L"edge",L"microsoft-edge"},
        {L"7zip",L"7zip"},{L"peazip",L"peazip"},{L"everything",L"everything"},{L"powertoys",L"powertoys"},{L"rufus",L"rufus"},{L"winscp",L"winscp"},
        {L"vscode",L"vscode"},{L"git",L"git"},{L"github-desktop",L"github-desktop"},{L"python",L"python312"},{L"nodejs",L"nodejs-lts"},{L"cmake",L"cmake"},{L"notepadpp",L"notepadplusplus"},
        {L"vlc",L"vlc"},{L"obs",L"obs-studio"},{L"audacity",L"audacity"},{L"handbrake",L"handbrake"},{L"spotify",L"spotify"},{L"gimp",L"gimp"},
        {L"steam",L"steam"},{L"epic-games",L"epicgameslauncher"},{L"gog-galaxy",L"goggalaxy"},{L"discord",L"discord"},{L"playnite",L"playnite"},{L"prismlauncher",L"prismlauncher"},
        {L"teams",L"microsoft-teams"},{L"zoom",L"zoom"},{L"slack",L"slack"},{L"telegram",L"telegram"},{L"signal",L"signal"},{L"thunderbird",L"thunderbird"},
        {L"bitwarden",L"bitwarden"},{L"keepassxc",L"keepassxc"},{L"malwarebytes",L"malwarebytes"},{L"veracrypt",L"veracrypt"},{L"gpg4win",L"gpg4win"},
        {L"terminal",L"microsoft-windows-terminal"},{L"process-explorer",L"procexp"},{L"autoruns",L"autoruns"},{L"wireshark",L"wireshark"},{L"windirstat",L"windirstat"},{L"hwinfo",L"hwinfo"},{L"cpu-z",L"cpu-z"}
    };
    for(const auto& item:ids)if(_wcsicmp(item.slug,package.slug)==0)return item.id;
    return nullptr;
}

std::wstring lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return value;
}

bool ieq(const std::wstring& a, const wchar_t* b) { return _wcsicmp(a.c_str(), b) == 0; }

const Package* find_slug(const std::wstring& slug) {
    for (const auto& package : kCatalog) if (ieq(slug, package.slug)) return &package;
    return nullptr;
}

std::vector<const Package*> filter_category(const std::wstring& category) {
    std::vector<const Package*> result;
    for (const auto& package : kCatalog) if (ieq(category, package.category)) result.push_back(&package);
    return result;
}

std::vector<const Package*> search_catalog(const std::wstring& query) {
    std::vector<const Package*> result;
    const std::wstring needle = lower(query);
    for (const auto& p : kCatalog) {
        std::wstring text = std::wstring(p.slug) + L"\n" + p.name + L"\n" + p.id + L"\n" + p.category + L"\n" + p.description;
        if (lower(text).find(needle) != std::wstring::npos) result.push_back(&p);
    }
    return result;
}

void enable_console_ui() {
    SetConsoleTitleW(L"Nebula Store");
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE); DWORD mode = 0;
    if (GetConsoleMode(output, &mode)) SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

void clear_screen() { std::wcout << L"\x1b[2J\x1b[H"; }
bool console_input() { DWORD mode = 0; return GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode) != FALSE; }

void print_help() {
    std::wcout
        << L"n10store - Nebula10 curated Chocolatey application store\n"
        << L"Usage:\n"
        << L"  n10store [menu]\n"
        << L"  n10store --help\n"
        << L"  n10store list [category]\n"
        << L"  n10store search QUERY\n"
        << L"  n10store info SLUG\n"
        << L"  n10store install SLUG [--dry-run] [--yes]\n"
        << L"  n10store upgrade SLUG [--dry-run] [--yes]\n"
        << L"  n10store uninstall SLUG [--dry-run] [--yes]\n"
        << L"  n10store choco-status\n"
        << L"  n10store setup-choco [--dry-run]\n\n"
        << L"Only slugs from the compiled catalog are accepted. Categories:\n  ";
    for (size_t i = 0; i < sizeof(kCategories) / sizeof(kCategories[0]); ++i)
        std::wcout << (i ? L", " : L"") << kCategories[i];
    std::wcout << L"\n";
}

void print_package(const Package& p) {
    std::wcout << L"Name       : " << p.name << L"\n"
               << L"Slug       : " << p.slug << L"\n"
               << L"Category   : " << p.category << L"\n"
               << L"Chocolatey : " << (choco_id(p)?choco_id(p):L"Unavailable") << L"\n"
               << L"Description: " << p.description << L"\n";
}

int print_list(const std::vector<const Package*>& packages) {
    if (packages.empty()) { std::wcout << L"No catalog entries matched.\n"; return 1; }
    std::wcout << L"Nebula Store catalog (" << packages.size() << L" entries)\n";
    std::wcout << L"SLUG | NAME | CHOCOLATEY ID | CATEGORY\n";
    std::wcout << L"--------------------------------------------------------------------------------\n";
    for (const Package* p : packages) std::wcout << p->slug << L" | " << p->name << L" | " << (choco_id(*p)?choco_id(*p):L"Unavailable") << L" | " << p->category << L"\n";
    return 0;
}

std::wstring windows_error(DWORD code) {
    wchar_t* raw = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, 0, reinterpret_cast<wchar_t*>(&raw), 0, nullptr);
    std::wstring text = raw ? raw : L"Windows error " + std::to_wstring(code);
    if (raw) LocalFree(raw);
    while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n')) text.pop_back();
    return text;
}

std::wstring resolve_choco() {
    wchar_t programData[32768]{};DWORD n=GetEnvironmentVariableW(L"ProgramData",programData,32768);
    if(n&&n<32768){std::wstring installed=std::wstring(programData)+L"\\Nebula10\\Chocolatey\\choco.exe";if(GetFileAttributesW(installed.c_str())!=INVALID_FILE_ATTRIBUTES)return installed;}
    std::wstring packaged=nebula::exe_dir()+L"\\Tools\\choco\\choco.exe";
    if(GetFileAttributesW(packaged.c_str())!=INVALID_FILE_ATTRIBUTES)return packaged;
    return L"";
}

std::wstring quote_argument(const std::wstring& value) {
    if (value.empty()) return L"\"\"";
    if (value.find_first_of(L" \t\n\v\"") == std::wstring::npos) return value;
    std::wstring out = L"\""; size_t slashes = 0;
    for (wchar_t c : value) {
        if (c == L'\\') { ++slashes; continue; }
        if (c == L'\"') { out.append(slashes * 2 + 1, L'\\'); out += c; slashes = 0; continue; }
        out.append(slashes, L'\\'); slashes = 0; out += c;
    }
    out.append(slashes * 2, L'\\'); out += L'\"';
    return out;
}

std::wstring display_command(const std::wstring& exe, const std::vector<std::wstring>& args) {
    std::wstring command = quote_argument(exe);
    for (const auto& arg : args) command += L" " + quote_argument(arg);
    return command;
}

int setup_choco(bool dry_run){
    std::wstring packaged=nebula::exe_dir()+L"\\Tools\\choco\\choco.exe";
    if(dry_run){std::wcout<<L"DRY-RUN: would initialize the supplied Tools\\choco runtime at "<<packaged<<L".\nNo package changes made.\n";return 0;}
    std::wstring exe=resolve_choco();if(exe.empty()){std::wcerr<<L"Supplied Chocolatey runtime is missing. Repair Nebula10 from the complete package.\n";return 4;}
    std::wcout<<L"Chocolatey runtime ready: "<<exe<<L"\n";return 0;
}

int run_choco(const std::vector<std::wstring>& args, bool dry_run) {
    const std::wstring exe = resolve_choco();
    if (dry_run) {
        std::wstring preview=exe.empty()?nebula::exe_dir()+L"\\Tools\\choco\\choco.exe":exe;
        std::wcout << L"DRY-RUN: would run " << display_command(preview, args) << L"\n";
        std::wcout << L"No package changes made; no process or network request was started.\n";
        return 0;
    }
    if (exe.empty()) {
        std::wcerr << L"Nebula Chocolatey runtime was not found. Run setup-choco or repair Nebula10.\n";
        return 4;
    }
    std::wstring parameters;for(const auto& arg:args){if(!parameters.empty())parameters+=L" ";parameters+=quote_argument(arg);}std::wstring root=exe.substr(0,exe.find_last_of(L"\\/"));wchar_t old[32768]{};DWORD oldSize=GetEnvironmentVariableW(L"ChocolateyInstall",old,32768);SetEnvironmentVariableW(L"CHOCOLATEYINSTALL",nullptr);SetEnvironmentVariableW(L"ChocolateyInstall",root.c_str());
    SHELLEXECUTEINFOW launch{};launch.cbSize=sizeof(launch);launch.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_NOASYNC;launch.lpVerb=L"runas";launch.lpFile=exe.c_str();launch.lpParameters=parameters.c_str();launch.lpDirectory=root.c_str();launch.nShow=SW_SHOWNORMAL;
    BOOL started=ShellExecuteExW(&launch);
    if(oldSize&&oldSize<32768)SetEnvironmentVariableW(L"ChocolateyInstall",old);else SetEnvironmentVariableW(L"ChocolateyInstall",nullptr);
    if (!started) {
        DWORD error = GetLastError();
        std::wcerr << L"Could not start bundled Chocolatey: " << windows_error(error) << L" (" << error << L")\n";
        return error==ERROR_CANCELLED?1223:5;
    }
    WaitForSingleObject(launch.hProcess, INFINITE);
    DWORD exit_code = 1;
    if (!GetExitCodeProcess(launch.hProcess, &exit_code)) exit_code = 1;
    CloseHandle(launch.hProcess);
    return static_cast<int>(exit_code);
}

int run_choco_readonly(const std::vector<std::wstring>& args){
    const std::wstring exe=resolve_choco();if(exe.empty())return 4;std::wstring command=quote_argument(exe);for(const auto&arg:args)command+=L" "+quote_argument(arg);std::vector<wchar_t> buffer(command.begin(),command.end());buffer.push_back(0);
    std::wstring root=exe.substr(0,exe.find_last_of(L"\\/"));wchar_t old[32768]{};DWORD oldSize=GetEnvironmentVariableW(L"ChocolateyInstall",old,32768);SetEnvironmentVariableW(L"CHOCOLATEYINSTALL",nullptr);SetEnvironmentVariableW(L"ChocolateyInstall",root.c_str());STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};BOOL started=CreateProcessW(exe.c_str(),buffer.data(),nullptr,nullptr,TRUE,0,nullptr,root.c_str(),&startup,&process);if(oldSize&&oldSize<32768)SetEnvironmentVariableW(L"ChocolateyInstall",old);else SetEnvironmentVariableW(L"ChocolateyInstall",nullptr);if(!started)return 5;WaitForSingleObject(process.hProcess,INFINITE);DWORD code{};GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hThread);CloseHandle(process.hProcess);return static_cast<int>(code);
}

bool confirm_action(const std::wstring& action, const Package& package, bool assume_yes) {
    if (assume_yes) return true;
    std::wcout << L"Confirm " << action << L" of " << package.name << L" (" << package.id << L")? [y/N]: ";
    std::wstring answer;
    if (!std::getline(std::wcin, answer)) return false;
    return ieq(answer, L"y") || ieq(answer, L"yes");
}

int package_action(const std::wstring& action, const Package& package, bool dry_run, bool assume_yes) {
    if (!dry_run && !confirm_action(action, package, assume_yes)) {
        std::wcout << L"Cancelled.\n";
        return 0;
    }
    const wchar_t* id=choco_id(package);if(!id){std::wcerr<<L"No allowlisted Chocolatey package exists for this entry.\n";return 4;}
    if(!dry_run&&!nebula::require_nebula_integrity(L"N10Store.exe"))return 8;
    std::vector<std::wstring> args = {action, id, L"-y", L"--no-progress"};
    return run_choco(args, dry_run);
}

struct MenuEntry { std::wstring primary; std::wstring secondary; };

void draw_menu(const std::wstring& title, const std::vector<MenuEntry>& entries, size_t selected, bool logo) {
    clear_screen();
    if (logo) std::wcout
        << L"\x1b[96m"
        << L"    _   ____________  __  ____    ___ \n"
        << L"   / | / / ____/ __ )/ / / / /   /   |\n"
        << L"  /  |/ / __/ / __  / / / / /   / /| |\n"
        << L" / /|  / /___/ /_/ / /_/ / /___/ ___ |\n"
        << L"/_/ |_/_____/_____/\\____/_____/_/  |_|\n"
        << L"\x1b[0m                       NEBULA STORE\n";
    std::wcout << L"  __________________________________________________________________________\n\n  " << title << L"\n\n";
    const size_t visibleRows = logo ? 11 : 15;
    size_t start = selected >= visibleRows ? selected - visibleRows + 1 : 0;
    if (entries.size() > visibleRows && start + visibleRows > entries.size()) start = entries.size() - visibleRows;
    const size_t end = std::min(entries.size(), start + visibleRows);
    if (start) std::wcout << L"    \x1b[90m... more above ...\x1b[0m\n";
    for (size_t i = start; i < end; ++i) {
        std::wcout << (i == selected ? L"\x1b[92m  > " : L"    ") << entries[i].primary;
        if (i == selected) std::wcout << L"\x1b[0m";
        if (!entries[i].secondary.empty()) std::wcout << L"  \x1b[90m[" << entries[i].secondary << L"]\x1b[0m";
        std::wcout << L"\n";
    }
    if (end < entries.size()) std::wcout << L"    \x1b[90m... more below ...\x1b[0m\n";
    if (entries.size() > visibleRows) std::wcout << L"\n  Showing " << (start + 1) << L"-" << end << L" of " << entries.size() << L"\n";
    std::wcout << L"\n  __________________________________________________________________________\n\n"
                  L"  UP/DOWN or W/S navigate   ENTER select   ESC back\n";
}

int select_menu(const std::wstring& title, const std::vector<MenuEntry>& entries, bool logo = false) {
    if (entries.empty()) return -1;
    size_t selected = 0; const bool interactive = console_input();
    for (;;) {
        draw_menu(title, entries, selected, logo);
        if (!interactive) {
            std::wcout << L"  Select (1-" << entries.size() << L", 0 to go back): ";
            std::wstring input; if (!std::getline(std::wcin, input)) return -1;
            if (input == L"0") return -1;
            try { size_t n = std::stoul(input); if (n >= 1 && n <= entries.size()) return static_cast<int>(n - 1); } catch (...) {}
            return -1;
        }
        int key = _getwch();
        if (key == 0 || key == 224) {
            int scan = _getwch();
            if (scan == 72) selected = selected ? selected - 1 : entries.size() - 1;
            else if (scan == 80) selected = (selected + 1) % entries.size();
            continue;
        }
        if (key == L'w' || key == L'W') selected = selected ? selected - 1 : entries.size() - 1;
        else if (key == L's' || key == L'S') selected = (selected + 1) % entries.size();
        else if (key == 13) return static_cast<int>(selected);
        else if (key == 27) return -1;
    }
}

void pause_action() {
    std::wcout << L"\nPress any key to return...";
    if (console_input()) _getwch(); else { std::wstring ignored; std::getline(std::wcin, ignored); }
}

int package_menu(const Package& package, bool dry_run) {
    const std::vector<MenuEntry> actions = {
        {L"Information", package.description}, {L"Install", L"Install exact catalog package"},
        {L"Upgrade", L"Upgrade exact catalog package"}, {L"Uninstall", L"Remove exact catalog package"},
        {L"Back", L"Return to catalog"}
    };
    for (;;) {
        int selected = select_menu(std::wstring(package.name) + L"  [" + package.slug + L"]", actions);
        if (selected < 0 || selected == 4) return 0;
        clear_screen(); int rc = 0;
        if (selected == 0) print_package(package);
        else rc = package_action(selected == 1 ? L"install" : selected == 2 ? L"upgrade" : L"uninstall", package, dry_run, false);
        if (rc) std::wcout << L"\nAction returned code " << rc << L".\n";
        pause_action();
    }
}

void browse_menu(const std::wstring& title, const std::vector<const Package*>& packages, bool dry_run) {
    std::vector<MenuEntry> entries;
    for (const Package* p : packages) entries.push_back({p->name, std::wstring(p->slug) + L" · " + p->category});
    entries.push_back({L"Back", L"Return to Nebula Store"});
    for (;;) {
        int selected = select_menu(title, entries);
        if (selected < 0 || static_cast<size_t>(selected) == packages.size()) return;
        package_menu(*packages[static_cast<size_t>(selected)],dry_run);
    }
}

void category_menu(bool dry_run) {
    std::vector<MenuEntry> entries;
    for (const wchar_t* category : kCategories) {
        size_t count = filter_category(category).size();
        entries.push_back({category, std::to_wstring(count) + L" applications"});
    }
    entries.push_back({L"Back", L"Return to Nebula Store"});
    for (;;) {
        int selected = select_menu(L"Categories", entries);
        if (selected < 0 || static_cast<size_t>(selected) == sizeof(kCategories) / sizeof(kCategories[0])) return;
        browse_menu(kCategories[selected], filter_category(kCategories[selected]),dry_run);
    }
}

void search_menu(bool dry_run) {
    clear_screen(); std::wcout << L"Search the curated catalog\n\nQuery (blank to cancel): ";
    std::wstring query; if (!std::getline(std::wcin, query) || query.empty()) return;
    auto matches = search_catalog(query);
    if (matches.empty()) { std::wcout << L"\nNo catalog entries matched \"" << query << L"\".\n"; pause_action(); return; }
    browse_menu(L"Search results for \"" + query + L"\"", matches,dry_run);
}

int run_tui(bool dry_run) {
    enable_console_ui();
    const std::vector<MenuEntry> main = {
        {L"Browse all applications", std::to_wstring(kCatalogSize) + L" curated entries"},
        {L"Browse categories", L"Browsers, utilities, development, media, and more"},
        {L"Search", L"Find by name, slug, ID, category, or description"},
        {L"Set up bundled Chocolatey", L"Use only Nebula's packaged Chocolatey runtime"},
        {L"Chocolatey status", L"Locate the folder-bound runtime and show its version"},
        {L"Command help", L"Automation and command-line usage"},
        {L"Exit", L"Close Nebula Store"}
    };
    std::vector<const Package*> all; for (const auto& p : kCatalog) all.push_back(&p);
    for (;;) {
        int selected = select_menu(L"Curated applications installed through folder-bound Chocolatey", main, true);
        if (selected < 0 || selected == 6) { clear_screen(); std::wcout << L"Goodbye from Nebula Store.\n"; return 0; }
        if (selected == 0) browse_menu(L"All applications", all,dry_run);
        else if (selected == 1) category_menu(dry_run);
        else if (selected == 2) search_menu(dry_run);
        else {
            clear_screen();
            int rc = 0;
            if(selected==3)rc=setup_choco(dry_run);
            else if (selected == 4) { std::wstring exe = resolve_choco(); std::wcout << (exe.empty() ? L"Nebula Chocolatey runtime is missing. Repair Nebula10.\n" : L"Chocolatey executable: " + exe + L"\nVersion: "); if (!exe.empty()) rc = run_choco_readonly({L"--version"}); }
            else print_help();
            if (rc) std::wcout << L"\nAction returned code " << rc << L".\n";
            pause_action();
        }
    }
}

int invalid_usage(const std::wstring& message) {
    std::wcerr << message << L"\nUse n10store --help for usage.\n";
    return 2;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    ensure_store_shortcuts();
    bool dry_run = false, assume_yes = false;
    std::vector<std::wstring> positional;
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (ieq(arg, L"--help") || ieq(arg, L"-h")) { print_help(); return 0; }
        if (ieq(arg, L"--dry-run")) { dry_run = true; continue; }
        if (ieq(arg, L"--yes")) { assume_yes = true; continue; }
        if (!arg.empty() && arg[0] == L'-') return invalid_usage(L"Unknown option: " + arg);
        positional.push_back(arg);
    }
    if (positional.empty() || ieq(positional[0], L"menu")) {
        if (positional.size() > 1 || assume_yes) return invalid_usage(L"menu accepts only --dry-run.");
        return run_tui(dry_run);
    }
    const std::wstring command = lower(positional[0]);
    if (command == L"list") {
        if (dry_run || assume_yes || positional.size() > 2) return invalid_usage(L"Usage: n10store list [category]");
        std::vector<const Package*> packages;
        if (positional.size() == 1) for (const auto& p : kCatalog) packages.push_back(&p);
        else {
            packages = filter_category(positional[1]);
            if (packages.empty()) return invalid_usage(L"Unknown category: " + positional[1]);
        }
        return print_list(packages);
    }
    if (command == L"search") {
        if (dry_run || assume_yes || positional.size() != 2 || positional[1].empty()) return invalid_usage(L"Usage: n10store search QUERY");
        return print_list(search_catalog(positional[1]));
    }
    if (command == L"info") {
        if (dry_run || assume_yes || positional.size() != 2) return invalid_usage(L"Usage: n10store info SLUG");
        const Package* p = find_slug(positional[1]);
        if (!p) return invalid_usage(L"Unknown catalog slug: " + positional[1]);
        print_package(*p); return 0;
    }
    if (command == L"setup-choco") {
        if (assume_yes || positional.size() != 1) return invalid_usage(L"Usage: n10store setup-choco [--dry-run]");
        return setup_choco(dry_run);
    }
    if (command == L"choco-status") {
        if (dry_run || assume_yes || positional.size() != 1) return invalid_usage(L"Usage: n10store choco-status");
        std::wstring exe = resolve_choco();
        if (exe.empty()) { std::wcout << L"Chocolatey runtime is missing. Repair Nebula10 from the complete package.\n"; return 0; }
        std::wcout << L"Chocolatey executable: " << exe << L"\nVersion: ";
        return run_choco_readonly({L"--version"});
    }
    if (command == L"install" || command == L"upgrade" || command == L"uninstall") {
        if (positional.size() != 2) return invalid_usage(L"Usage: n10store " + command + L" SLUG [--dry-run] [--yes]");
        const Package* p = find_slug(positional[1]);
        if (!p) return invalid_usage(L"Package is not in the curated N10Store catalog: " + positional[1] + L". Arbitrary package IDs are not allowed.");
        return package_action(command, *p, dry_run, assume_yes);
    }
    return invalid_usage(L"Unknown command: " + positional[0]);
}
