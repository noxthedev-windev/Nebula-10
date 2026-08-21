#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shobjidl.h>

#include <atomic>
#include <new>
#include <string>
#include <vector>

// {7E950195-94A7-4E5D-9C94-E51E8D0F94CD}
// Stable CLSID used to register the ForceOwn Explorer command handler.
extern "C" const CLSID CLSID_ForceOwnShell = {
    0x7e950195, 0x94a7, 0x4e5d, {0x9c, 0x94, 0xe5, 0x1e, 0x8d, 0x0f, 0x94, 0xcd}
};

namespace {

HMODULE g_module = nullptr;
std::atomic<long> g_liveObjects{0};
std::atomic<long> g_serverLocks{0};

class ModuleObject {
public:
    ModuleObject() noexcept { ++g_liveObjects; }
    ModuleObject(const ModuleObject&) = delete;
    ModuleObject& operator=(const ModuleObject&) = delete;

protected:
    ~ModuleObject() { --g_liveObjects; }
};

HRESULT duplicate_string(const wchar_t* text, LPWSTR* output) noexcept {
    if (!output) return E_POINTER;
    *output = nullptr;
    const SIZE_T bytes = (wcslen(text) + 1) * sizeof(wchar_t);
    auto* copy = static_cast<LPWSTR>(CoTaskMemAlloc(bytes));
    if (!copy) return E_OUTOFMEMORY;
    CopyMemory(copy, text, bytes);
    *output = copy;
    return S_OK;
}

IShellItemArray* effective_selection(IShellItemArray* supplied,
                                     IShellItemArray* stored) noexcept {
    return supplied ? supplied : stored;
}

HRESULT selection_count(IShellItemArray* supplied, IShellItemArray* stored,
                        DWORD* count) noexcept {
    if (!count) return E_POINTER;
    *count = 0;
    IShellItemArray* selection = effective_selection(supplied, stored);
    return selection ? selection->GetCount(count) : E_FAIL;
}

HRESULT item_is_folder(IShellItemArray* selection, bool* isFolder) noexcept {
    if (!selection || !isFolder) return E_INVALIDARG;
    *isFolder = false;

    IShellItem* item = nullptr;
    HRESULT hr = selection->GetItemAt(0, &item);
    if (FAILED(hr)) return hr;

    SFGAOF attributes = 0;
    hr = item->GetAttributes(SFGAO_FOLDER, &attributes);
    item->Release();
    if (SUCCEEDED(hr)) *isFolder = (attributes & SFGAO_FOLDER) != 0;
    return hr;
}

HRESULT filesystem_paths(IShellItemArray* selection,
                         std::vector<std::wstring>* paths) {
    if (!selection || !paths) return E_INVALIDARG;
    paths->clear();

    DWORD count = 0;
    HRESULT hr = selection->GetCount(&count);
    if (FAILED(hr)) return hr;
    if (count == 0) return E_INVALIDARG;

    paths->reserve(count);
    for (DWORD index = 0; index < count; ++index) {
        IShellItem* item = nullptr;
        hr = selection->GetItemAt(index, &item);
        if (FAILED(hr)) return hr;

        LPWSTR rawPath = nullptr;
        hr = item->GetDisplayName(SIGDN_FILESYSPATH, &rawPath);
        item->Release();
        if (FAILED(hr)) return hr;
        if (!rawPath || !*rawPath) {
            CoTaskMemFree(rawPath);
            return HRESULT_FROM_WIN32(ERROR_BAD_PATHNAME);
        }

        try {
            paths->emplace_back(rawPath);
        } catch (...) {
            CoTaskMemFree(rawPath);
            return E_OUTOFMEMORY;
        }
        CoTaskMemFree(rawPath);
    }
    return S_OK;
}

// Quote one argv element according to CommandLineToArgvW/CreateProcess rules.
std::wstring quote_argument(const std::wstring& argument) {
    std::wstring result;
    result.reserve(argument.size() + 2);
    result.push_back(L'"');

    size_t backslashes = 0;
    for (const wchar_t character : argument) {
        if (character == L'\\') {
            ++backslashes;
        } else if (character == L'"') {
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(L'"');
            backslashes = 0;
        } else {
            result.append(backslashes, L'\\');
            result.push_back(character);
            backslashes = 0;
        }
    }
    result.append(backslashes * 2, L'\\');
    result.push_back(L'"');
    return result;
}

HRESULT installed_executable(std::wstring* executable) {
    if (!executable) return E_POINTER;
    executable->clear();
    if (!g_module) return E_UNEXPECTED;

    std::vector<wchar_t> modulePath(32768);
    const DWORD length = GetModuleFileNameW(
        g_module, modulePath.data(), static_cast<DWORD>(modulePath.size()));
    if (length == 0) return HRESULT_FROM_WIN32(GetLastError());
    if (length >= modulePath.size() - 1) {
        return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    }

    std::wstring path(modulePath.data(), length);
    const size_t separator = path.find_last_of(L"\\/");
    if (separator == std::wstring::npos) return E_UNEXPECTED;
    path.resize(separator + 1);
    path += L"n10forceown.exe";

    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    *executable = std::move(path);
    return S_OK;
}

HRESULT launch_forceown(const std::vector<std::wstring>& paths) {
    std::wstring executable;
    HRESULT hr = installed_executable(&executable);
    if (FAILED(hr)) return hr;

    // The option terminator ensures every selected path is parsed only as data.
    std::wstring commandLine = quote_argument(executable) + L" --shell --";
    for (const auto& path : paths) {
        commandLine.push_back(L' ');
        commandLine += quote_argument(path);
    }

    std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
    mutableCommandLine.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), mutableCommandLine.data(), nullptr,
                        nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr, nullptr,
                        &startup, &process)) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return S_OK;
}

class ForceOwnCommand final : public IExplorerCommand,
                              public IObjectWithSelection,
                              private ModuleObject {
public:
    ForceOwnCommand() noexcept = default;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (IsEqualIID(iid, IID_IUnknown) || IsEqualIID(iid, IID_IExplorerCommand)) {
            *object = static_cast<IExplorerCommand*>(this);
        } else if (IsEqualIID(iid, IID_IObjectWithSelection)) {
            *object = static_cast<IObjectWithSelection*>(this);
        } else {
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++references_; }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG remaining = --references_;
        if (remaining == 0) delete this;
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE GetTitle(IShellItemArray* supplied,
                                       LPWSTR* title) override {
        if (!title) return E_POINTER;
        *title = nullptr;

        DWORD count = 0;
        HRESULT hr = selection_count(supplied, selection_, &count);
        if (FAILED(hr)) return hr;
        if (count > 1) return duplicate_string(L"ForceOwn all", title);
        if (count == 0) return E_INVALIDARG;

        bool folder = false;
        hr = item_is_folder(effective_selection(supplied, selection_), &folder);
        if (FAILED(hr)) return hr;
        return duplicate_string(folder ? L"ForceOwn this folder"
                                       : L"ForceOwn this file", title);
    }

    HRESULT STDMETHODCALLTYPE GetIcon(IShellItemArray*, LPWSTR* icon) override {
        if (!icon) return E_POINTER;
        *icon = nullptr;
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetToolTip(IShellItemArray*, LPWSTR* tooltip) override {
        if (!tooltip) return E_POINTER;
        *tooltip = nullptr;
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE GetCanonicalName(GUID* commandName) override {
        if (!commandName) return E_POINTER;
        *commandName = CLSID_ForceOwnShell;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetState(IShellItemArray* supplied, BOOL,
                                       EXPCMDSTATE* state) override {
        if (!state) return E_POINTER;
        DWORD count = 0;
        const HRESULT hr = selection_count(supplied, selection_, &count);
        *state = (SUCCEEDED(hr) && count != 0) ? ECS_ENABLED : ECS_HIDDEN;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Invoke(IShellItemArray* supplied, IBindCtx*) override {
        try {
            IShellItemArray* selection = effective_selection(supplied, selection_);
            std::vector<std::wstring> paths;
            const HRESULT hr = filesystem_paths(selection, &paths);
            return SUCCEEDED(hr) ? launch_forceown(paths) : hr;
        } catch (const std::bad_alloc&) {
            return E_OUTOFMEMORY;
        } catch (...) {
            return E_FAIL;
        }
    }

    HRESULT STDMETHODCALLTYPE GetFlags(EXPCMDFLAGS* flags) override {
        if (!flags) return E_POINTER;
        *flags = ECF_DEFAULT;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE EnumSubCommands(IEnumExplorerCommand** commands) override {
        if (!commands) return E_POINTER;
        *commands = nullptr;
        return E_NOTIMPL;
    }

    HRESULT STDMETHODCALLTYPE SetSelection(IShellItemArray* selection) override {
        if (selection) selection->AddRef();
        if (selection_) selection_->Release();
        selection_ = selection;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetSelection(REFIID iid, void** object) override {
        if (!object) return E_POINTER;
        *object = nullptr;
        return selection_ ? selection_->QueryInterface(iid, object) : E_FAIL;
    }

private:
    ~ForceOwnCommand() {
        if (selection_) selection_->Release();
    }

    std::atomic<ULONG> references_{1};
    IShellItemArray* selection_ = nullptr;
};

class ForceOwnClassFactory final : public IClassFactory, private ModuleObject {
public:
    ForceOwnClassFactory() noexcept = default;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (!IsEqualIID(iid, IID_IUnknown) && !IsEqualIID(iid, IID_IClassFactory)) {
            return E_NOINTERFACE;
        }
        *object = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++references_; }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG remaining = --references_;
        if (remaining == 0) delete this;
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer, REFIID iid,
                                             void** object) override {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (outer) return CLASS_E_NOAGGREGATION;

        auto* command = new (std::nothrow) ForceOwnCommand();
        if (!command) return E_OUTOFMEMORY;
        const HRESULT hr = command->QueryInterface(iid, object);
        command->Release();
        return hr;
    }

    HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override {
        if (lock) {
            ++g_serverLocks;
        } else {
            long current = g_serverLocks.load();
            while (current > 0 &&
                   !g_serverLocks.compare_exchange_weak(current, current - 1)) {
            }
        }
        return S_OK;
    }

private:
    ~ForceOwnClassFactory() = default;
    std::atomic<ULONG> references_{1};
};

}  // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}

extern "C" __declspec(dllexport) HRESULT __stdcall
DllGetClassObject(REFCLSID clsid, REFIID iid, void** object) {
    if (!object) return E_POINTER;
    *object = nullptr;
    if (!IsEqualCLSID(clsid, CLSID_ForceOwnShell)) return CLASS_E_CLASSNOTAVAILABLE;

    auto* factory = new (std::nothrow) ForceOwnClassFactory();
    if (!factory) return E_OUTOFMEMORY;
    const HRESULT hr = factory->QueryInterface(iid, object);
    factory->Release();
    return hr;
}

extern "C" __declspec(dllexport) HRESULT __stdcall DllCanUnloadNow() {
    return (g_liveObjects.load() == 0 && g_serverLocks.load() == 0) ? S_OK : S_FALSE;
}
