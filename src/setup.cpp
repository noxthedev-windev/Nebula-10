#include "verinfo.hpp"
#include <shellapi.h>
#include <shlobj.h>
#include <objbase.h>
#include <filesystem>
#include <vector>

namespace fs=std::filesystem;
namespace {
using namespace nebula;
bool rebootRequired=false;
constexpr const wchar_t* kForceOwnClsid=L"{7E950195-94A7-4E5D-9C94-E51E8D0F94CD}";
constexpr const wchar_t* kForceOwnCommandKey=L"SOFTWARE\\Classes\\AllFilesystemObjects\\shell\\NebulaForceOwn";

struct InstallSelection {
    bool core=true;
    bool store=true;
    bool tools=true;
};

void print_install_selection(const InstallSelection&selection){
    std::wcout<<L"Selected components:\n"
      L"  Core: selected (required)\n"
      L"  Nebula Store: "<<(selection.store?L"selected":L"not selected")<<L"\n"
      L"  Local Tools: "<<(selection.tools?L"selected":L"not selected")<<L"\n";
}

bool parse_install_components(int argc,wchar_t**argv,InstallSelection&selection,bool&specified){
    specified=false;
    for(int i=1;i<argc;i++){
        std::wstring argument=argv[i];
        if(argument==L"--components"){std::wcerr<<L"Use --components=core,store,tools.\n";return false;}
        if(argument.rfind(L"--components=",0)!=0)continue;
        if(specified){std::wcerr<<L"--components may only be specified once.\n";return false;}
        specified=true;selection={true,false,false};
        std::wstring value=argument.substr(13);bool seenCore=false,seenStore=false,seenTools=false;
        size_t start=0;
        while(true){
            size_t comma=value.find(L',',start);std::wstring item=value.substr(start,comma==std::wstring::npos?comma:comma-start);
            bool*seen=nullptr;
            if(item==L"core")seen=&seenCore;
            else if(item==L"store")seen=&seenStore;
            else if(item==L"tools")seen=&seenTools;
            else {std::wcerr<<L"Invalid component '"<<item<<L"'. Use core, store, and/or tools.\n";return false;}
            if(*seen){std::wcerr<<L"Duplicate component '"<<item<<L"'.\n";return false;}
            *seen=true;
            if(comma==std::wstring::npos)break;
            start=comma+1;
        }
        if(!seenCore){std::wcerr<<L"Core is required and must be included in --components.\n";return false;}
        selection.store=seenStore;selection.tools=seenTools;
    }
    return true;
}

bool prompt_component(const wchar_t*name){
    while(true){
        std::wcout<<L"Install "<<name<<L"? [Y/n] "<<std::flush;std::wstring answer;
        if(!std::getline(std::wcin,answer)||answer.empty()||answer==L"y"||answer==L"Y"||answer==L"yes"||answer==L"YES")return true;
        if(answer==L"n"||answer==L"N"||answer==L"no"||answer==L"NO")return false;
        std::wcout<<L"Please enter y or n.\n";
    }
}

InstallSelection select_install_components(){
    InstallSelection selection;DWORD mode{};
    if(GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),&mode)){
        std::wcout<<L"\nChoose optional Nebula components. Core is required.\n";
        selection.store=prompt_component(L"Nebula Store");
        selection.tools=prompt_component(L"quality-of-life tools from the Tools folder");
    }
    print_install_selection(selection);return selection;
}

bool admin(){
    BOOL value=FALSE;PSID sid{};SID_IDENTIFIER_AUTHORITY nt=SECURITY_NT_AUTHORITY;
    if(AllocateAndInitializeSid(&nt,2,SECURITY_BUILTIN_DOMAIN_RID,DOMAIN_ALIAS_RID_ADMINS,0,0,0,0,0,0,&sid)){
        CheckTokenMembership(nullptr,sid,&value);FreeSid(sid);
    }
    return value!=FALSE;
}
std::wstring target(){wchar_t p[MAX_PATH]{};GetEnvironmentVariableW(L"ProgramFiles",p,MAX_PATH);return std::wstring(p)+L"\\Nebula10";}
void banner(const ModInfo&m,const WindowsInfo&w){
    std::wcout<<L"============================================================\n"
      L"                       NEBULA SETUP                         \n"
      L"                       by NoxTheDev                      \n"
      L"============================================================\n"
      <<nebula_identity(m,w)<<L"\nCodename: "<<m.codename<<L" | Version: "<<m.modVersion<<L" | Channel: "<<m.channel<<L"\n\n";
}
void help(){
    std::wcout<<L"NebulaSetup - terminal installer for the Nebula Windows customization pack\n"
      L"Usage: NebulaSetup [--install|--repair|--uninstall] [--components=core,store,tools] [--dry-run] [--no-pause] [--help]\n"
      L"Install/repair always deploys Core; Nebula Store and Local Tools are optional and selected by default.\n"
      L"Use --components with a comma-separated list for unattended selection; core is required.\n"
      L"Troubleshooting: NebulaSetup --copy-diagnostics <destination-directory> <payload-file>\n";
}
bool shortcut(const std::wstring& link,const std::wstring& exe,const std::wstring& args=L"",const std::wstring& icon=L""){
    IShellLinkW*s{};if(FAILED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_IShellLinkW,reinterpret_cast<void**>(&s))))return false;
    s->SetPath(exe.c_str());s->SetArguments(args.c_str());s->SetWorkingDirectory(target().c_str());if(!icon.empty())s->SetIconLocation(icon.c_str(),0);IPersistFile*p{};
    bool ok=SUCCEEDED(s->QueryInterface(IID_IPersistFile,reinterpret_cast<void**>(&p)))&&SUCCEEDED(p->Save(link.c_str(),TRUE));if(p)p->Release();s->Release();return ok;
}
void remove_legacy_n10_shortcuts(){
    PWSTR desktop{},programs{};
    if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop,0,nullptr,&desktop))){DeleteFileW((std::wstring(desktop)+L"\\N10 Version.lnk").c_str());CoTaskMemFree(desktop);}
    if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_CommonPrograms,0,nullptr,&programs))){std::wstring folder=std::wstring(programs)+L"\\Nebula10";DeleteFileW((folder+L"\\N10 Version.lnk").c_str());DeleteFileW((folder+L"\\Uninstall Nebula10.lnk").c_str());CoTaskMemFree(programs);}
}

void remove_file_context_menus(){
    for(const auto&key:{L"SOFTWARE\\Classes\\*\\shell\\NebulaForceOwn",L"SOFTWARE\\Classes\\Directory\\shell\\NebulaForceOwn",L"SOFTWARE\\Classes\\Directory\\Background\\shell\\NebulaForceOwn",L"SOFTWARE\\Classes\\*\\shell\\NebulaHash",L"SOFTWARE\\Classes\\*\\shell\\NebulaPathInfo",L"SOFTWARE\\Classes\\Directory\\shell\\NebulaPathInfo",L"SOFTWARE\\Classes\\*\\shell\\NebulaLockCheck"})RegDeleteTreeW(HKEY_LOCAL_MACHINE,key);
    SHChangeNotify(SHCNE_ASSOCCHANGED,SHCNF_IDLIST,nullptr,nullptr);
}
void delete_registry_tree_64(const std::wstring&path){
    HKEY key{};
    if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,path.c_str(),0,KEY_WRITE|KEY_WOW64_64KEY,&key)==ERROR_SUCCESS){RegDeleteTreeW(key,nullptr);RegCloseKey(key);}
    RegDeleteKeyExW(HKEY_LOCAL_MACHINE,path.c_str(),KEY_WOW64_64KEY,0);
}
void remove_forceown_shell_registration(){
    delete_registry_tree_64(kForceOwnCommandKey);
    std::wstring clsidKey=L"SOFTWARE\\Classes\\CLSID\\"+std::wstring(kForceOwnClsid);
    delete_registry_tree_64(clsidKey);
    HKEY approved{};
    if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",0,KEY_SET_VALUE|KEY_WOW64_64KEY,&approved)==ERROR_SUCCESS){RegDeleteValueW(approved,kForceOwnClsid);RegCloseKey(approved);}
    SHChangeNotify(SHCNE_ASSOCCHANGED,SHCNF_IDLIST,nullptr,nullptr);
}
bool set_registry_string(HKEY root,const std::wstring&path,const wchar_t*name,const std::wstring&value){
    HKEY key{};DWORD disposition{};
    if(RegCreateKeyExW(root,path.c_str(),0,nullptr,0,KEY_SET_VALUE|KEY_WOW64_64KEY,nullptr,&key,&disposition)!=ERROR_SUCCESS)return false;
    LONG result=RegSetValueExW(key,name,0,REG_SZ,reinterpret_cast<const BYTE*>(value.c_str()),static_cast<DWORD>((value.size()+1)*sizeof(wchar_t)));RegCloseKey(key);return result==ERROR_SUCCESS;
}
bool register_forceown_context_menu(const std::wstring&destination){
    const std::wstring clsid=kForceOwnClsid;
    bool ok=set_registry_string(HKEY_LOCAL_MACHINE,kForceOwnCommandKey,L"ExplorerCommandHandler",clsid)
      &&set_registry_string(HKEY_LOCAL_MACHINE,kForceOwnCommandKey,L"MultiSelectModel",L"Player")
      &&set_registry_string(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Classes\\CLSID\\"+clsid+L"\\InprocServer32",nullptr,destination+L"\\NebulaForceOwnShell.dll")
      &&set_registry_string(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Classes\\CLSID\\"+clsid+L"\\InprocServer32",L"ThreadingModel",L"Apartment")
      &&set_registry_string(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",kForceOwnClsid,L"Nebula ForceOwn Explorer command");
    if(!ok)remove_forceown_shell_registration();else SHChangeNotify(SHCNE_ASSOCCHANGED,SHCNF_IDLIST,nullptr,nullptr);
    return ok;
}
bool service_install(const std::wstring& dir){
    SC_HANDLE manager=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_ALL_ACCESS);if(!manager)return false;
    std::wstring bin=L"\""+dir+L"\\NebulaUserAuthService.exe\"";
    SC_HANDLE service=CreateServiceW(manager,kService,L"Nebula10 User Authorization Service",SERVICE_ALL_ACCESS,SERVICE_WIN32_OWN_PROCESS,SERVICE_AUTO_START,SERVICE_ERROR_NORMAL,bin.c_str(),nullptr,nullptr,nullptr,nullptr,nullptr);
    if(!service&&GetLastError()==ERROR_SERVICE_EXISTS)service=OpenServiceW(manager,kService,SERVICE_ALL_ACCESS);
    bool ok=service!=nullptr;if(service){ChangeServiceConfigW(service,SERVICE_NO_CHANGE,SERVICE_AUTO_START,SERVICE_NO_CHANGE,bin.c_str(),nullptr,nullptr,nullptr,nullptr,nullptr,nullptr);StartServiceW(service,0,nullptr);CloseServiceHandle(service);}CloseServiceHandle(manager);return ok;
}
bool service_stop_for_update(){
    SC_HANDLE manager=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_CONNECT);if(!manager)return false;
    SC_HANDLE service=OpenServiceW(manager,kService,SERVICE_STOP|SERVICE_QUERY_STATUS);if(!service){DWORD e=GetLastError();CloseServiceHandle(manager);return e==ERROR_SERVICE_DOES_NOT_EXIST;}
    SERVICE_STATUS status{};ControlService(service,SERVICE_CONTROL_STOP,&status);
    bool stopped=false;for(int i=0;i<50;i++){SERVICE_STATUS_PROCESS state{};DWORD needed{};if(QueryServiceStatusEx(service,SC_STATUS_PROCESS_INFO,reinterpret_cast<BYTE*>(&state),sizeof(state),&needed)&&state.dwCurrentState==SERVICE_STOPPED){stopped=true;break;}Sleep(100);}
    CloseServiceHandle(service);CloseServiceHandle(manager);return stopped;
}
void service_remove(){
    SC_HANDLE manager=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_CONNECT);if(!manager)return;SC_HANDLE service=OpenServiceW(manager,kService,SERVICE_STOP|DELETE|SERVICE_QUERY_STATUS);
    if(service){SERVICE_STATUS status{};ControlService(service,SERVICE_CONTROL_STOP,&status);DeleteService(service);CloseServiceHandle(service);}CloseServiceHandle(manager);
}
void finish_and_pause(bool success,bool pause){
    std::wcout<<(success?L"\nFinished.\n":L"\nFinished with errors.\n");
    DWORD mode{};if(pause&&GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),&mode)){
        std::wcout<<L"Press Enter to close..."<<std::flush;std::wstring input;std::getline(std::wcin,input);
    }
}
bool machine_path(const std::wstring& dir,bool remove){
    HKEY key{};if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment",0,KEY_QUERY_VALUE|KEY_SET_VALUE,&key)!=ERROR_SUCCESS)return false;
    wchar_t buffer[32768]{};DWORD size=sizeof(buffer),type{};RegQueryValueExW(key,L"Path",nullptr,&type,reinterpret_cast<BYTE*>(buffer),&size);std::wstring path=buffer;
    auto pos=path.find(dir);if(remove&&pos!=path.npos){size_t end=pos+dir.size();if(end<path.size()&&path[end]==L';')end++;else if(pos&&path[pos-1]==L';')pos--;path.erase(pos,end-pos);}
    else if(!remove&&pos==path.npos){if(!path.empty()&&path.back()!=L';')path+=L';';path+=dir;}
    LONG result=RegSetValueExW(key,L"Path",0,REG_EXPAND_SZ,reinterpret_cast<BYTE*>(path.data()),static_cast<DWORD>((path.size()+1)*sizeof(wchar_t)));RegCloseKey(key);
    SendMessageTimeoutW(HWND_BROADCAST,WM_SETTINGCHANGE,0,reinterpret_cast<LPARAM>(L"Environment"),SMTO_ABORTIFHUNG,3000,nullptr);return result==ERROR_SUCCESS;
}
bool backup_oem_value(HKEY oem,const wchar_t* name,const wchar_t* id){
    HKEY backup{};DWORD disposition{};if(RegCreateKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Nebula10\\SetupBackup",0,nullptr,0,KEY_QUERY_VALUE|KEY_SET_VALUE|KEY_WOW64_64KEY,nullptr,&backup,&disposition)!=ERROR_SUCCESS)return false;
    std::wstring marker=std::wstring(id)+L"Saved";DWORD value{},n=sizeof(value),type{};
    if(RegQueryValueExW(backup,marker.c_str(),nullptr,&type,reinterpret_cast<BYTE*>(&value),&n)==ERROR_SUCCESS){RegCloseKey(backup);return true;}
    DWORD oldType{},oldSize{};LONG query=RegQueryValueExW(oem,name,nullptr,&oldType,nullptr,&oldSize);DWORD existed=query==ERROR_SUCCESS?1u:0u;std::vector<BYTE> data(oldSize);
    if(existed&&RegQueryValueExW(oem,name,nullptr,&oldType,data.data(),&oldSize)!=ERROR_SUCCESS){RegCloseKey(backup);return false;}
    RegSetValueExW(backup,(std::wstring(id)+L"Existed").c_str(),0,REG_DWORD,reinterpret_cast<BYTE*>(&existed),sizeof(existed));
    RegSetValueExW(backup,(std::wstring(id)+L"Type").c_str(),0,REG_DWORD,reinterpret_cast<BYTE*>(&oldType),sizeof(oldType));
    if(existed)RegSetValueExW(backup,(std::wstring(id)+L"Data").c_str(),0,REG_BINARY,data.data(),oldSize);
    value=1;LONG result=RegSetValueExW(backup,marker.c_str(),0,REG_DWORD,reinterpret_cast<BYTE*>(&value),sizeof(value));RegCloseKey(backup);return result==ERROR_SUCCESS;
}
bool restore_oem_value(HKEY oem,const wchar_t* name,const wchar_t* id){
    HKEY backup{};if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Nebula10\\SetupBackup",0,KEY_QUERY_VALUE|KEY_SET_VALUE|KEY_WOW64_64KEY,&backup)!=ERROR_SUCCESS)return true;
    std::wstring base=id,marker=base+L"Saved";DWORD markerValue{},n=sizeof(DWORD),type{};
    if(RegQueryValueExW(backup,marker.c_str(),nullptr,&type,reinterpret_cast<BYTE*>(&markerValue),&n)!=ERROR_SUCCESS){RegCloseKey(backup);return true;}
    DWORD existed{},originalType{};
    n=sizeof(DWORD);RegQueryValueExW(backup,(base+L"Existed").c_str(),nullptr,&type,reinterpret_cast<BYTE*>(&existed),&n);
    n=sizeof(DWORD);RegQueryValueExW(backup,(base+L"Type").c_str(),nullptr,&type,reinterpret_cast<BYTE*>(&originalType),&n);
    LONG result{};if(existed){DWORD size{};RegQueryValueExW(backup,(base+L"Data").c_str(),nullptr,nullptr,nullptr,&size);std::vector<BYTE> data(size);RegQueryValueExW(backup,(base+L"Data").c_str(),nullptr,nullptr,data.data(),&size);result=RegSetValueExW(oem,name,0,originalType,data.data(),size);}else{result=RegDeleteValueW(oem,name);if(result==ERROR_FILE_NOT_FOUND)result=ERROR_SUCCESS;}
    if(result==ERROR_SUCCESS){RegDeleteValueW(backup,marker.c_str());RegDeleteValueW(backup,(base+L"Existed").c_str());RegDeleteValueW(backup,(base+L"Type").c_str());RegDeleteValueW(backup,(base+L"Data").c_str());}RegCloseKey(backup);return result==ERROR_SUCCESS;
}
std::wstring settings_oem_model(const ModInfo&m,const WindowsInfo&w){
    std::wstring edition=w.product.empty()?w.edition:w.product;
    for(const std::wstring prefix:{L"Microsoft ",L"Windows 11 ",L"Windows 10 ",L"Windows "}){
        size_t pos{};while((pos=edition.find(prefix))!=std::wstring::npos)edition.erase(pos,prefix.size());
    }
    while(!edition.empty()&&edition.front()==L' ')edition.erase(edition.begin());
    if(edition.empty())edition=L"Host edition";
    return L"Nebula 10 "+m.buildId+L" | "+edition+L" | Build "+w.build+L"."+std::to_wstring(w.ubr);
}
bool apply_branding(const ModInfo&m,const WindowsInfo&w){
    HKEY oem{};DWORD disposition{};if(RegCreateKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\OEMInformation",0,nullptr,0,KEY_QUERY_VALUE|KEY_SET_VALUE|KEY_WOW64_64KEY,nullptr,&oem,&disposition)!=ERROR_SUCCESS)return false;
    bool ok=backup_oem_value(oem,L"Manufacturer",L"Manufacturer")&&backup_oem_value(oem,L"Model",L"Model");
    std::wstring manufacturer=L"NoxTheDev",model=settings_oem_model(m,w);
    if(ok)ok=RegSetValueExW(oem,L"Manufacturer",0,REG_SZ,reinterpret_cast<const BYTE*>(manufacturer.c_str()),static_cast<DWORD>((manufacturer.size()+1)*sizeof(wchar_t)))==ERROR_SUCCESS;
    if(ok)ok=RegSetValueExW(oem,L"Model",0,REG_SZ,reinterpret_cast<const BYTE*>(model.c_str()),static_cast<DWORD>((model.size()+1)*sizeof(wchar_t)))==ERROR_SUCCESS;
    RegCloseKey(oem);
    HKEY identity{};if(RegCreateKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Nebula10\\Identity",0,nullptr,0,KEY_SET_VALUE|KEY_WOW64_64KEY,nullptr,&identity,&disposition)==ERROR_SUCCESS){
        std::wstring installRoot=target();const std::pair<const wchar_t*,const std::wstring*> values[]={{L"ModName",&m.modName},{L"ModVersion",&m.modVersion},{L"BuildId",&m.buildId},{L"Codename",&m.codename},{L"Channel",&m.channel},{L"SupportedWindowsBuilds",&m.supportedBuilds},{L"InstallRoot",&installRoot}};
        for(const auto&v:values)RegSetValueExW(identity,v.first,0,REG_SZ,reinterpret_cast<const BYTE*>(v.second->c_str()),static_cast<DWORD>((v.second->size()+1)*sizeof(wchar_t)));
        RegCloseKey(identity);
    }else ok=false;return ok;
}
void restore_branding(){
    HKEY oem{};
    if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\OEMInformation",0,KEY_SET_VALUE|KEY_WOW64_64KEY,&oem)==ERROR_SUCCESS){restore_oem_value(oem,L"Manufacturer",L"Manufacturer");restore_oem_value(oem,L"Model",L"Model");RegCloseKey(oem);}RegDeleteTreeW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Nebula10\\Identity");RegDeleteTreeW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Nebula10\\Integrity");
}
std::vector<std::wstring> files(const InstallSelection&selection){
    std::vector<std::wstring> result={L"n10ver.exe",L"n10toolbox.exe",L"n10forceown.exe",L"NebulaForceOwnShell.dll",L"NebulaUserAuth.exe",L"NebulaUserAuthService.exe",L"NebulaSetup.exe",L"verinfo.bin",L"README.md",L"SECURITY.md",L"LICENSE",L"LICENSES.md"};
    if(selection.store)result.push_back(L"N10Store.exe");
    return result;
}
std::vector<std::wstring> installed_tool_files(){return {
    L"Tools\\choco\\choco.exe",L"Tools\\choco\\LICENSE.txt",L"Tools\\choco\\config\\chocolatey.config",
    L"Tools\\Mem Reduct\\memreduct.exe",L"Tools\\Mem Reduct\\License.txt",
    L"Tools\\OpenShell\\StartMenu.exe",L"Tools\\OpenShell\\StartMenuDLL.dll",L"Tools\\OpenShell\\OpenShellReadme.rtf",
    L"Tools\\WinXShell\\WinXShell.exe",L"Tools\\WinXShell\\WinXShell.jcfg",L"Tools\\WinXShell\\wxsStub.dll",
    L"Tools\\dwmblurglass\\DWMBlurGlass.exe",L"Tools\\dwmblurglass\\DWMBlurGlassExt.dll",L"Tools\\dwmblurglass\\data\\config.ini",
    L"Tools\\Explorer++.exe",L"Tools\\ShutUp10.exe",L"Tools\\neofetch.exe"};}
bool verify_installed_tools(const fs::path&destination){
    bool ok=true;std::error_code ec;
    for(const auto&relative:installed_tool_files()){
        fs::path path=destination/relative;
        if(!fs::is_regular_file(path,ec)||ec){std::wcerr<<L"Installed tool payload is incomplete: "<<path.wstring()<<L"\n";ok=false;}
        ec.clear();
    }
    return ok;
}
std::vector<std::wstring> integrity_files(const InstallSelection&selection){return files(selection);}
void remove_retired_file_tools(const fs::path& destination){
    for(const auto*name:{L"n10hash.exe",L"n10pathinfo.exe",L"n10locks.exe"}){fs::path path=destination/name;if(DeleteFileW(path.c_str()))std::wcout<<L"Removed retired tool: "<<path.wstring()<<L"\n";else if(GetLastError()!=ERROR_FILE_NOT_FOUND)MoveFileExW(path.c_str(),nullptr,MOVEFILE_DELAY_UNTIL_REBOOT);}
    remove_file_context_menus();
}
bool write_integrity_state(const fs::path& source,const InstallSelection&selection){
    HKEY key{};DWORD disposition{};if(RegCreateKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Nebula10\\Integrity",0,nullptr,0,KEY_SET_VALUE|KEY_WOW64_64KEY,nullptr,&key,&disposition)!=ERROR_SUCCESS)return false;
    if(!selection.store)RegDeleteValueW(key,L"N10Store.exe");
    bool ok=true;for(const auto&name:integrity_files(selection)){const auto extension=fs::path(name).extension();if(extension!=L".exe"&&extension!=L".dll"&&extension!=L".ps1")continue;std::wstring digest=sha256_file((source/name).wstring());if(digest.empty()||RegSetValueExW(key,name.c_str(),0,REG_SZ,reinterpret_cast<const BYTE*>(digest.c_str()),static_cast<DWORD>((digest.size()+1)*sizeof(wchar_t)))!=ERROR_SUCCESS)ok=false;}
    RegCloseKey(key);return ok;
}
std::vector<std::wstring> missing_runtime_payload(const fs::path&source,const InstallSelection&selection){
    std::vector<std::wstring> required={L"n10ver.exe",L"n10toolbox.exe",L"n10forceown.exe",L"NebulaForceOwnShell.dll",L"NebulaUserAuth.exe",L"NebulaUserAuthService.exe",L"verinfo.bin"};
    if(selection.store)required.push_back(L"N10Store.exe");
    if(selection.tools){for(const auto&item:installed_tool_files())required.push_back(item);}
    std::vector<std::wstring> missing;std::error_code ec;
    for(const auto&item:required){if(!fs::is_regular_file(source/item,ec)||ec)missing.push_back(item);ec.clear();}
    return missing;
}
bool payload_preflight(const fs::path&source,const InstallSelection&selection){
    auto missing=missing_runtime_payload(source,selection);if(missing.empty())return true;
    std::wcerr<<L"\nINCOMPLETE NEBULA10 PACKAGE\n"
      L"NebulaSetup.exe cannot be installed by itself. Missing required payload:\n";
    for(const auto&item:missing)std::wcerr<<L"  - "<<item<<L"\n";
    std::wcerr<<L"\nExtract the entire ZIP to a normal folder, keep all files together, then run NebulaSetup.exe again.\n"
      L"Do not run Setup from inside the ZIP preview.\nPackage folder: "<<source.wstring()<<L"\n";
    return false;
}
bool same_existing_path(const fs::path&a,const fs::path&b){
    std::error_code ec;if(!fs::exists(a,ec)||ec||!fs::exists(b,ec)||ec)return false;
    bool equivalent=fs::equivalent(a,b,ec);return !ec&&equivalent;
}
bool copy_payload_file(const fs::path&source,const fs::path&destination,bool verbose=true){
    if(same_existing_path(source,destination)){
        if(verbose)std::wcout<<L"Already in place: "<<destination.wstring()<<L"\n";
        return true;
    }
    std::error_code ec;if(!fs::exists(source,ec)||ec){
        std::wcerr<<L"Copy failed: source is missing\n  Source: "<<source.wstring()<<L"\n  Destination: "<<destination.wstring()<<L"\n";return false;
    }
    fs::create_directories(destination.parent_path(),ec);if(ec){
        std::wcerr<<L"Copy failed: could not create destination directory ("<<ec.value()<<L": "<<utf8_to_wide(ec.message())<<L")\n  Source: "<<source.wstring()<<L"\n  Destination: "<<destination.wstring()<<L"\n";return false;
    }
    if(!CopyFileW(source.c_str(),destination.c_str(),FALSE)){
        DWORD error=GetLastError();
        if((error==ERROR_SHARING_VIOLATION||error==ERROR_ACCESS_DENIED||error==ERROR_USER_MAPPED_FILE)&&fs::exists(destination,ec)){
            fs::path staged=destination;staged+=L".nebula-new";DeleteFileW(staged.c_str());
            if(CopyFileW(source.c_str(),staged.c_str(),FALSE)&&MoveFileExW(destination.c_str(),nullptr,MOVEFILE_DELAY_UNTIL_REBOOT)&&MoveFileExW(staged.c_str(),destination.c_str(),MOVEFILE_DELAY_UNTIL_REBOOT)){
                rebootRequired=true;std::wcout<<L"In use; replacement scheduled after reboot: "<<destination.wstring()<<L"\n";return true;
            }
            error=GetLastError();
        }
        std::error_code winError(static_cast<int>(error),std::system_category());
        std::wcerr<<L"Copy failed ("<<error<<L": "<<utf8_to_wide(winError.message())<<L")\n  Source: "<<source.wstring()<<L"\n  Destination: "<<destination.wstring()<<L"\n";return false;
    }
    if(verbose)std::wcout<<L"Copied: "<<destination.wstring()<<L"\n";
    return true;
}
bool copy_payload_tree(const fs::path&source,const fs::path&destination){
    if(same_existing_path(source,destination)){std::wcout<<L"Already in place: "<<destination.wstring()<<L"\n";return true;}
    std::error_code ec;if(!fs::is_directory(source,ec)||ec){std::wcerr<<L"Copy failed: payload directory is missing\n  Source: "<<source.wstring()<<L"\n  Destination: "<<destination.wstring()<<L"\n";return false;}
    bool ok=true;fs::create_directories(destination,ec);if(ec){std::wcerr<<L"Copy failed creating directory ("<<ec.value()<<L": "<<utf8_to_wide(ec.message())<<L"): "<<destination.wstring()<<L"\n";return false;}
    for(auto it=fs::recursive_directory_iterator(source,fs::directory_options::skip_permission_denied,ec);it!=fs::recursive_directory_iterator();it.increment(ec)){
        if(ec){std::wcerr<<L"Copy failed enumerating "<<source.wstring()<<L" ("<<ec.value()<<L": "<<utf8_to_wide(ec.message())<<L")\n";ok=false;ec.clear();continue;}
        fs::path relative=fs::relative(it->path(),source,ec);if(ec){ok=false;ec.clear();continue;}fs::path out=destination/relative;
        if(it->is_directory())fs::create_directories(out,ec);else if(it->is_regular_file())ok&=copy_payload_file(it->path(),out,false);
        if(ec){std::wcerr<<L"Copy failed for "<<out.wstring()<<L" ("<<ec.value()<<L": "<<utf8_to_wide(ec.message())<<L")\n";ok=false;ec.clear();}
    }
    return ok;
}
void schedule_tree_remove(const fs::path& root){
    std::error_code ec;if(!fs::exists(root,ec))return;std::vector<fs::path> dirs;
    for(auto it=fs::recursive_directory_iterator(root,fs::directory_options::skip_permission_denied,ec);it!=fs::recursive_directory_iterator();it.increment(ec)){if(it->is_directory())dirs.push_back(it->path());else MoveFileExW(it->path().c_str(),nullptr,MOVEFILE_DELAY_UNTIL_REBOOT);}
    std::sort(dirs.rbegin(),dirs.rend());for(const auto&d:dirs)MoveFileExW(d.c_str(),nullptr,MOVEFILE_DELAY_UNTIL_REBOOT);MoveFileExW(root.c_str(),nullptr,MOVEFILE_DELAY_UNTIL_REBOOT);
}
void remove_retired_tool_payloads(const fs::path&destination){
    const fs::path toolsRoot=destination/L"Tools";
    const std::vector<fs::path> retired={
      L"optimizerNXT.exe",L"forceown-cmenu.exe",L"finaltweaks.exe",L"checkstart.exe",
      L"parkcontrolsetup64.exe",L"processlassosetup64.exe",L"fulldiskfix.cmd",L"cmdloader.bat",
      L"tweaks",L"skus_win",L"just-the-browser",L"programs",
      fs::path(L"OpenShell")/L"currentuser.reg",fs::path(L"OpenShell")/L"localmachine.reg",
      fs::path(L"OpenShell")/L"Start Menu Settings.lnk",fs::path(L"OpenShell")/L"Start Screen.lnk",
      fs::path(L"choco")/L"config"/L"chocolatey.config.backup"};
    for(const auto&relative:retired){
        fs::path path=toolsRoot/relative;std::error_code ec;
        if(!fs::exists(path,ec)||ec)continue;
        fs::remove_all(path,ec);
        if(ec){schedule_tree_remove(path);rebootRequired=true;}
        else std::wcout<<L"Removed retired managed tool payload: "<<path.wstring()<<L"\n";
    }
}
bool remove_managed_file(const fs::path&path){
    if(DeleteFileW(path.c_str())||GetLastError()==ERROR_FILE_NOT_FOUND)return true;
    if(MoveFileExW(path.c_str(),nullptr,MOVEFILE_DELAY_UNTIL_REBOOT)){rebootRequired=true;return true;}
    std::wcerr<<L"Could not remove unselected managed file: "<<path.wstring()<<L"\n";return false;
}
bool remove_store_shortcuts(){
    bool ok=true;PWSTR desktop{},programs{};
    if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop,0,nullptr,&desktop))){ok&=remove_managed_file(fs::path(desktop)/L"Nebula Store.lnk");CoTaskMemFree(desktop);}
    if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_CommonPrograms,0,nullptr,&programs))){ok&=remove_managed_file(fs::path(programs)/L"Nebula10"/L"Nebula Store.lnk");CoTaskMemFree(programs);}
    return ok;
}
void remove_managed_tools(const fs::path&destination){
    fs::path tools=destination/L"Tools";std::error_code ec;
    if(!fs::exists(tools,ec)||ec)return;
    fs::remove_all(tools,ec);
    if(ec){schedule_tree_remove(tools);rebootRequired=true;}
    else std::wcout<<L"Removed unselected managed Local Tools.\n";
}
}

int wmain(int argc,wchar_t**argv){
    bool dry=has_arg(argc,argv,L"--dry-run"),noPause=has_arg(argc,argv,L"--no-pause");if(has_arg(argc,argv,L"--help")||has_arg(argc,argv,L"-h")){help();return 0;}
    if(has_arg(argc,argv,L"--copy-diagnostics")){
        if(argc!=4){std::wcerr<<L"Usage: NebulaSetup --copy-diagnostics <destination-directory> <payload-file>\n";return 2;}
        fs::path source=fs::path(exe_dir())/argv[3],destination=fs::path(argv[2])/argv[3];bool ok=copy_payload_file(source,destination);
        std::wcout<<L"Payload copy diagnostics: "<<(ok?L"PASS":L"FAIL")<<L"\n";return ok?0:6;
    }
    bool uninstall=has_arg(argc,argv,L"--uninstall");std::wstring operation=uninstall?L"uninstall":has_arg(argc,argv,L"--repair")?L"repair":L"install";
    InstallSelection selection;bool componentsSpecified=false;
    if(!parse_install_components(argc,argv,selection,componentsSpecified))return 2;
    if(uninstall&&componentsSpecified){std::wcerr<<L"--components is only valid with --install or --repair.\n";return 2;}
    if(!uninstall&&!componentsSpecified&&!dry)selection=select_install_components();
    if(!uninstall&&!payload_preflight(exe_dir(),selection))return 4;
    ModInfo mod;WindowsInfo windows=get_windows_info();std::wstring manifest=exe_dir()+L"\\verinfo.bin",error;load_verinfo(manifest,mod,&error);banner(mod,windows);std::wstring destination=target();
    if(dry){
        if(!uninstall)print_install_selection(selection);
        std::wcout<<L"DRY-RUN "<<operation<<L"\nTarget: "<<destination<<L"\n"
          <<L"Would "<<(uninstall?L"restore OEM branding and remove":L"deploy")<<L" native applications, verinfo.bin, Nebula identity, OEM branding, service, machine PATH, Desktop and Start Menu shortcuts.\n";
        if(!uninstall)std::wcout<<L"Settings OEM model: "<<settings_oem_model(mod,windows)<<L"\n";
        if(!uninstall){
            if(selection.store)std::wcout<<L"Shortcuts: Nebula ToolBox and Nebula Store on Desktop and Start Menu; N10 Version remains command-line only.\nToolBox icon: Assets\\Branding\\NebulaToolBox.ico.\nStore icon: Assets\\Branding\\N10Store.ico.\n";
            else std::wcout<<L"Shortcuts: Nebula ToolBox on Desktop and Start Menu; the managed Nebula Store shortcuts would be removed. N10 Version remains command-line only.\nToolBox icon: Assets\\Branding\\NebulaToolBox.ico.\n";
        }
        std::wcout<<(uninstall?L"Downloaded Store data in C:\\Windows\\NebulaData\\Store would be preserved.\n":L"ForceOwn Explorer labels are dynamic: ForceOwn this file, ForceOwn this folder, and ForceOwn all.\n");
        std::wcout<<L"No changes made.\n";finish_and_pause(true,!noPause);return 0;
    }
    if(!admin()){
        wchar_t self[MAX_PATH]{};GetModuleFileNameW(nullptr,self,MAX_PATH);std::wstring args=L"--"+operation;
        if(!uninstall){args+=L" --components=core";if(selection.store)args+=L",store";if(selection.tools)args+=L",tools";}
        if(noPause)args+=L" --no-pause";
        auto result=reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"runas",self,args.c_str(),nullptr,SW_SHOWNORMAL));return result>32?0:5;
    }
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(uninstall){
        service_remove();machine_path(destination,true);restore_branding();remove_forceown_shell_registration();remove_file_context_menus();remove_legacy_n10_shortcuts();PWSTR desktop{},programs{};
        if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop,0,nullptr,&desktop))){DeleteFileW((std::wstring(desktop)+L"\\Nebula10 Toolbox.lnk").c_str());DeleteFileW((std::wstring(desktop)+L"\\Nebula Store.lnk").c_str());CoTaskMemFree(desktop);}
        if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_CommonPrograms,0,nullptr,&programs))){fs::remove_all(fs::path(programs)/L"Nebula10");CoTaskMemFree(programs);}schedule_tree_remove(destination);
        std::wcout<<L"Nebula10 uninstalled; prior OEM branding restored. Downloaded Store data in C:\\Windows\\NebulaData\\Store was preserved. Locked files will be removed after reboot.\n";CoUninitialize();finish_and_pause(true,!noPause);return 0;
    }
    if(!service_stop_for_update())std::wcout<<L"UserAuth service could not be stopped; a locked service binary will be replaced after reboot.\n";
    remove_forceown_shell_registration();
    fs::create_directories(destination);auto source=exe_dir();bool ok=true;
    for(const auto&file:files(selection))ok&=copy_payload_file(fs::path(source)/file,fs::path(destination)/file);
    if(!selection.store){ok&=remove_managed_file(fs::path(destination)/L"N10Store.exe");ok&=remove_store_shortcuts();}
    ok&=copy_payload_tree(fs::path(source)/L"Assets",fs::path(destination)/L"Assets");
    if(selection.tools){ok&=copy_payload_tree(fs::path(source)/L"Tools",fs::path(destination)/L"Tools");remove_retired_tool_payloads(destination);ok&=verify_installed_tools(destination);}
    else remove_managed_tools(destination);
    remove_retired_file_tools(destination);
    ok&=register_forceown_context_menu(destination);
    ok&=apply_branding(mod,windows);
    if(selection.store){
        const wchar_t* storeRoots[]={L"C:\\Windows\\NebulaData",L"C:\\Windows\\NebulaData\\Store",L"C:\\Windows\\NebulaData\\Store\\Downloads",L"C:\\Windows\\NebulaData\\Store\\Cache"};
        for(const wchar_t* root:storeRoots){int created=SHCreateDirectoryExW(nullptr,root,nullptr);if(created!=ERROR_SUCCESS&&created!=ERROR_ALREADY_EXISTS&&created!=ERROR_FILE_EXISTS){std::wcerr<<L"Could not create Store data folder "<<root<<L" (error "<<created<<L").\n";ok=false;}}
        if(ok)std::wcout<<L"Store data folders ready under C:\\Windows\\NebulaData\\Store.\n";
    }
    ok&=write_integrity_state(source,selection);
    ok&=service_install(destination);ok&=machine_path(destination,false);remove_legacy_n10_shortcuts();PWSTR desktop{},programs{};std::wstring toolboxIcon=destination+L"\\Assets\\Branding\\NebulaToolBox.ico",storeIcon=destination+L"\\Assets\\Branding\\N10Store.ico";
    if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop,0,nullptr,&desktop))){ok&=shortcut(std::wstring(desktop)+L"\\Nebula10 Toolbox.lnk",destination+L"\\n10toolbox.exe",L"",toolboxIcon);if(selection.store)ok&=shortcut(std::wstring(desktop)+L"\\Nebula Store.lnk",destination+L"\\N10Store.exe",L"",storeIcon);CoTaskMemFree(desktop);}
    if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_CommonPrograms,0,nullptr,&programs))){std::wstring folder=std::wstring(programs)+L"\\Nebula10";fs::create_directories(folder);ok&=shortcut(folder+L"\\Nebula10 Toolbox.lnk",destination+L"\\n10toolbox.exe",L"",toolboxIcon);if(selection.store)ok&=shortcut(folder+L"\\Nebula Store.lnk",destination+L"\\N10Store.exe",L"",storeIcon);CoTaskMemFree(programs);}
    CoUninitialize();std::wcout<<(ok?L"Nebula 10 installation complete. Open a new terminal and run n10toolbox or n10ver.\n":L"Nebula Setup completed with errors; review the messages above.\n");
    if(rebootRequired)std::wcout<<L"Restart Windows to finish replacing files that were in use.\n";
    finish_and_pause(ok,!noPause);return ok?0:6;
}
