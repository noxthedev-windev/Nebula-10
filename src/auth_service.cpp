#include "common.hpp"
#include <sddl.h>
#include <fstream>
#include <vector>

static SERVICE_STATUS_HANDLE g_status{};
static HANDLE g_stop{};
// Duplicated here intentionally so policy tests can audit the service boundary
// without relying only on common.hpp.
static constexpr const wchar_t* kStrictActions[] = {
    L"LONG_PATHS_ON", L"LONG_PATHS_OFF", L"OEM_BRANDING_ON", L"OEM_BRANDING_OFF"
};

static std::wstring data_root() {
    wchar_t p[MAX_PATH]{};
    DWORD n = GetEnvironmentVariableW(L"ProgramData", p, MAX_PATH);
    std::wstring d = (n ? std::wstring(p) : L"C:\\ProgramData") + L"\\Nebula10";
    CreateDirectoryW(d.c_str(), nullptr);
    return d;
}

static void log_line(const std::wstring& s) {
    std::wofstream f((data_root() + L"\\service.log").c_str(), std::ios::app);
    SYSTEMTIME t{}; GetSystemTime(&t);
    f << t.wYear << L"-" << t.wMonth << L"-" << t.wDay << L" "
      << t.wHour << L":" << t.wMinute << L":" << t.wSecond << L" " << s << L"\n";
}

static const wchar_t* backup_name(const std::wstring& action) {
    return action.find(L"LONG_PATHS") == 0 ? L"LongPaths" : L"OemManufacturer";
}

static bool save_original(HKEY target, const wchar_t* valueName, const std::wstring& action) {
    HKEY backup{};
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Nebula10\\Backups", 0, nullptr, 0,
                        KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &backup, nullptr) != ERROR_SUCCESS) return false;
    std::wstring marker = std::wstring(backup_name(action)) + L"Saved";
    DWORD markerValue{}, markerSize = sizeof(markerValue), markerType{};
    if (RegQueryValueExW(backup, marker.c_str(), nullptr, &markerType,
                         reinterpret_cast<BYTE*>(&markerValue), &markerSize) == ERROR_SUCCESS) {
        RegCloseKey(backup); return true;
    }
    DWORD type{}, size{};
    LONG q = RegQueryValueExW(target, valueName, nullptr, &type, nullptr, &size);
    DWORD existed = q == ERROR_SUCCESS ? 1u : 0u;
    std::vector<BYTE> data(size);
    if (existed && RegQueryValueExW(target, valueName, nullptr, &type, data.data(), &size) != ERROR_SUCCESS) {
        RegCloseKey(backup); return false;
    }
    std::wstring base = backup_name(action);
    LONG r = RegSetValueExW(backup, (base + L"Existed").c_str(), 0, REG_DWORD,
                            reinterpret_cast<const BYTE*>(&existed), sizeof(existed));
    if (r == ERROR_SUCCESS) r = RegSetValueExW(backup, (base + L"Type").c_str(), 0, REG_DWORD,
                                                reinterpret_cast<const BYTE*>(&type), sizeof(type));
    if (r == ERROR_SUCCESS && existed) r = RegSetValueExW(backup, (base + L"Data").c_str(), 0, REG_BINARY,
                                                           data.data(), size);
    markerValue = 1;
    if (r == ERROR_SUCCESS) r = RegSetValueExW(backup, marker.c_str(), 0, REG_DWORD,
                                                reinterpret_cast<const BYTE*>(&markerValue), sizeof(markerValue));
    RegCloseKey(backup);
    return r == ERROR_SUCCESS;
}

static bool restore_original(HKEY target, const wchar_t* valueName, const std::wstring& action) {
    HKEY backup{};
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Nebula10\\Backups", 0,
                      KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY, &backup) != ERROR_SUCCESS) return false;
    std::wstring base = backup_name(action), marker = base + L"Saved";
    DWORD existed{}, type{}, n = sizeof(DWORD), regType{};
    if (RegQueryValueExW(backup, marker.c_str(), nullptr, &regType, reinterpret_cast<BYTE*>(&existed), &n) != ERROR_SUCCESS) {
        RegCloseKey(backup); return false;
    }
    n = sizeof(DWORD);
    RegQueryValueExW(backup, (base + L"Existed").c_str(), nullptr, &regType, reinterpret_cast<BYTE*>(&existed), &n);
    n = sizeof(DWORD);
    RegQueryValueExW(backup, (base + L"Type").c_str(), nullptr, &regType, reinterpret_cast<BYTE*>(&type), &n);
    LONG r = ERROR_SUCCESS;
    if (existed) {
        DWORD size{};
        if (RegQueryValueExW(backup, (base + L"Data").c_str(), nullptr, &regType, nullptr, &size) != ERROR_SUCCESS) r = ERROR_FILE_NOT_FOUND;
        else {
            std::vector<BYTE> data(size);
            if (RegQueryValueExW(backup, (base + L"Data").c_str(), nullptr, &regType, data.data(), &size) != ERROR_SUCCESS) r = ERROR_READ_FAULT;
            else r = RegSetValueExW(target, valueName, 0, type, data.data(), size);
        }
    } else {
        r = RegDeleteValueW(target, valueName);
        if (r == ERROR_FILE_NOT_FOUND) r = ERROR_SUCCESS;
    }
    if (r == ERROR_SUCCESS) {
        RegDeleteValueW(backup, marker.c_str());
        RegDeleteValueW(backup, (base + L"Existed").c_str());
        RegDeleteValueW(backup, (base + L"Type").c_str());
        RegDeleteValueW(backup, (base + L"Data").c_str());
    }
    RegCloseKey(backup);
    return r == ERROR_SUCCESS;
}

static bool apply(const std::wstring& a) {
    HKEY k{}; const wchar_t* path{}; const wchar_t* name{};
    bool enable = a.size() >= 3 && a.substr(a.size() - 3) == L"_ON";
    if (a.find(L"LONG_PATHS") == 0) {
        path = L"SYSTEM\\CurrentControlSet\\Control\\FileSystem"; name = L"LongPathsEnabled";
    } else if (a.find(L"OEM_BRANDING") == 0) {
        path = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\OEMInformation"; name = L"Manufacturer";
    } else return false;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, path, 0, nullptr, 0,
                        KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &k, nullptr) != ERROR_SUCCESS) return false;
    bool ok{};
    if (enable) {
        ok = save_original(k, name, a);
        if (ok && a.find(L"LONG_PATHS") == 0) {
            DWORD one = 1; ok = RegSetValueExW(k, name, 0, REG_DWORD,
                                               reinterpret_cast<const BYTE*>(&one), sizeof(one)) == ERROR_SUCCESS;
        } else if (ok) {
            const wchar_t* s = L"NoxTheDev - Nebula10";
            ok = RegSetValueExW(k, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(s),
                                static_cast<DWORD>((wcslen(s) + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
        }
    } else {
        ok = restore_original(k, name, a);
    }
    RegCloseKey(k);
    return ok;
}

static bool validate_client(HANDLE pipe) {
    if (!ImpersonateNamedPipeClient(pipe)) return false;
    HANDLE token{}; BOOL member = FALSE;
    bool ok = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &token) != FALSE;
    BYTE sidBuffer[SECURITY_MAX_SID_SIZE]{}; DWORD sidSize = sizeof(sidBuffer);
    if (ok) ok = CreateWellKnownSid(WinInteractiveSid, nullptr, sidBuffer, &sidSize) != FALSE;
    if (ok) ok = CheckTokenMembership(token, sidBuffer, &member) != FALSE && member;
    if (token) CloseHandle(token);
    RevertToSelf();
    return ok;
}

static HANDLE create_pipe() {
    PSECURITY_DESCRIPTOR sd{};
    // LocalSystem/admin full access; local interactive desktop users may submit allowlisted requests.
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGW;;;IU)", SDDL_REVISION_1, &sd, nullptr)) return INVALID_HANDLE_VALUE;
    SECURITY_ATTRIBUTES sa{sizeof(sa), sd, FALSE};
    HANDLE p = CreateNamedPipeW(nebula::kPipe, PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        4, 512, 512, 1000, &sa);
    LocalFree(sd);
    return p;
}

static void serve() {
    while (WaitForSingleObject(g_stop, 0) == WAIT_TIMEOUT) {
        HANDLE p = create_pipe();
        if (p == INVALID_HANDLE_VALUE) { Sleep(1000); continue; }
        BOOL connected = ConnectNamedPipe(p, nullptr) ? TRUE : GetLastError() == ERROR_PIPE_CONNECTED;
        if (connected && validate_client(p)) {
            wchar_t b[128]{}; DWORD got{};
            if (ReadFile(p, b, sizeof(b) - sizeof(wchar_t), &got, nullptr)) {
                std::wstring a(b, got / sizeof(wchar_t));
                while (!a.empty() && (a.back() == L'\n' || a.back() == L'\r' || a.back() == 0)) a.pop_back();
                std::wstring reply;
                if (!nebula::action_allowed(a)) reply = L"ERROR rejected action\n";
                else reply = apply(a) ? L"OK action applied\n" : L"ERROR action failed or no backup exists\n";
                ULONG pid{}; GetNamedPipeClientProcessId(p, &pid);
                log_line(L"pid=" + std::to_wstring(pid) + L" " + a + L" -> " + reply);
                DWORD sent{}; WriteFile(p, reply.data(), static_cast<DWORD>(reply.size() * sizeof(wchar_t)), &sent, nullptr);
            }
        } else if (connected) log_line(L"Rejected non-interactive pipe client");
        FlushFileBuffers(p); DisconnectNamedPipe(p); CloseHandle(p);
    }
}

static void WINAPI ctrl(DWORD c) {
    if (c == SERVICE_CONTROL_STOP) {
        SERVICE_STATUS s{SERVICE_WIN32_OWN_PROCESS, SERVICE_STOP_PENDING, 0, 0, 0, 0, 0};
        SetServiceStatus(g_status, &s); SetEvent(g_stop);
    }
}
static void WINAPI service_main(DWORD, wchar_t**) {
    g_status = RegisterServiceCtrlHandlerW(nebula::kService, ctrl); if (!g_status) return;
    g_stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!nebula::require_nebula_integrity(L"NebulaUserAuthService.exe")) {
        log_line(L"Service refused to start: Nebula identity or SHA-256 integrity check failed.");
        SERVICE_STATUS failed{SERVICE_WIN32_OWN_PROCESS, SERVICE_STOPPED, 0, ERROR_INVALID_DATA, 0, 0, 0};
        SetServiceStatus(g_status, &failed); CloseHandle(g_stop); return;
    }
    SERVICE_STATUS s{SERVICE_WIN32_OWN_PROCESS, SERVICE_RUNNING, SERVICE_ACCEPT_STOP, NO_ERROR, 0, 0, 0};
    SetServiceStatus(g_status, &s); serve(); s.dwCurrentState = SERVICE_STOPPED; s.dwControlsAccepted = 0;
    SetServiceStatus(g_status, &s); CloseHandle(g_stop);
}
int wmain(int argc, wchar_t** argv) {
    if (nebula::has_arg(argc, argv, L"--help")) {
        std::wcout << L"NebulaUserAuthService - LocalSystem allowlisted named-pipe service\n"
                      L"Local interactive clients only. Strict actions: LONG_PATHS_ON/OFF, OEM_BRANDING_ON/OFF.\n";
        return 0;
    }
    SERVICE_TABLE_ENTRYW table[] = {{const_cast<wchar_t*>(nebula::kService), service_main}, {nullptr, nullptr}};
    if (!StartServiceCtrlDispatcherW(table)) {
        std::wcerr << L"Service dispatcher failed: " << GetLastError() << L"\n"; return 1;
    }
    return 0;
}
