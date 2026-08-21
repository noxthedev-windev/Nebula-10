#include "common.hpp"
#include "verinfo.hpp"
#include <shellapi.h>
#include <shlobj.h>
#include <conio.h>
#include <functional>
#include <iomanip>
#include <vector>

namespace {
using namespace nebula;

void enable_console_ui(){
    SetConsoleTitleW(L"Nebula ToolBox");
    HANDLE h=GetStdHandle(STD_OUTPUT_HANDLE);DWORD mode{};
    if(GetConsoleMode(h,&mode))SetConsoleMode(h,mode|ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}
void clear_screen(){std::wcout<<L"\x1b[2J\x1b[H";}
void help(){
    std::wcout<<L"n10toolbox - Nebula10 interactive terminal toolbox\n"
      L"Usage: n10toolbox [menu|COMMAND] [argument] [--dry-run]\n"
      L"Commands:\n"
      L"  menu, info, doctor, diagnostics <summary|storage|battery|display>\n"
      L"  privacy, balanced, poweruser, wallpaper <file>, rollback\n"
      L"  network <list|connections|adapters|firewall|proxy|wifi>\n"
      L"  tool <list|settings|taskmgr|terminal|control|events|services|devices|apps|dxdiag|resource\n"
      L"        |computer|diskmgmt|registry|sysinfo|reliability|performance|environment|features|security|power>\n"
      L"  maintenance <list|diskcleanup|optimize|storage|update|recovery|startup|troubleshoot|backup|restore>\n"
      L"  localtool <list|launch|enable|disable> [memreduct|openshell|winxshell|dwmblur|explorerpp|shutup10|neofetch]\n"
      L"  themes <help|roots|list|update|daily|daily-remove|select-wallpaper|select-icons> [pack]\n"
      L"  assets, logs, request <allowlisted-action>\n"
      L"Companion application: N10Store.\n"
      L"Run n10toolbox with no arguments to open the TUI.\n";
}
bool launch(const wchar_t* target,const wchar_t* args=L""){
    return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",target,args,nullptr,SW_SHOWNORMAL))>32;
}
bool path_exists(const std::wstring& path){return GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES;}
struct LocalTool{const wchar_t* id;const wchar_t* name;const wchar_t* relative;const wchar_t* description;};
static const LocalTool kLocalTools[]={
 {L"memreduct",L"Mem Reduct",L"Tools\\Mem Reduct\\memreduct.exe",L"Interactive memory working-set utility"},
 {L"openshell",L"OpenShell",L"Tools\\OpenShell\\StartMenu.exe",L"Start menu configuration"},
 {L"winxshell",L"WinXShell",L"Tools\\WinXShell\\WinXShell.exe",L"Alternative shell UI components"},
 {L"dwmblur",L"DWMBlurGlass",L"Tools\\dwmblurglass\\DWMBlurGlass.exe",L"Desktop composition appearance"},
 {L"explorerpp",L"Explorer++",L"Tools\\Explorer++.exe",L"Portable file manager"},
 {L"shutup10",L"ShutUp10",L"Tools\\ShutUp10.exe",L"Advanced privacy UI; review every change"},
 {L"neofetch",L"NeoFetch",L"Tools\\neofetch.exe",L"Terminal system summary"}
};
const LocalTool* find_local_tool(const std::wstring&id){for(const auto&tool:kLocalTools)if(_wcsicmp(id.c_str(),tool.id)==0)return &tool;return nullptr;}
bool local_tool_enabled(const LocalTool&tool){return reg_dword(HKEY_CURRENT_USER,L"Software\\Nebula10\\ToolPreferences",(std::wstring(tool.id)+L"Enabled").c_str(),1)!=0;}
bool set_local_tool_enabled(const LocalTool&tool,bool enabled){return set_hkcu_dword(L"Software\\Nebula10\\ToolPreferences",(std::wstring(tool.id)+L"Enabled").c_str(),enabled?1:0);}
int local_tool_command(const std::vector<std::wstring>&args,bool dry){
    if(args.size()<2||args[1]==L"list"){std::wcout<<L"Nebula local tools\n";for(const auto&tool:kLocalTools)std::wcout<<L"  "<<tool.id<<L"  "<<tool.name<<L"  ["<<(local_tool_enabled(tool)?L"Enabled":L"Disabled")<<L"]\n";return 0;}
    if(args.size()<3)return 2;
    const LocalTool*tool=find_local_tool(args[2]);if(!tool){std::wcerr<<L"Unknown local tool.\n";return 2;}
    if(args[1]==L"enable"||args[1]==L"disable"){bool enabled=args[1]==L"enable";if(dry){std::wcout<<L"DRY-RUN: would set "<<tool->name<<L" to "<<(enabled?L"Enabled":L"Disabled")<<L".\n";return 0;}return set_local_tool_enabled(*tool,enabled)?0:5;}
    if(args[1]!=L"launch")return 2;
    if(!local_tool_enabled(*tool)){std::wcerr<<tool->name<<L" is disabled in ToolPreferences.\n";return 6;}
    std::wstring path=exe_dir()+L"\\"+tool->relative;if(dry){std::wcout<<L"DRY-RUN: would launch "<<tool->name<<L" from "<<path<<L".\n";return 0;}if(!require_nebula_integrity(L"n10toolbox.exe"))return 8;if(!path_exists(path)){std::wcerr<<L"Tool payload missing: "<<path<<L"\n";return 4;}return launch(path.c_str())?0:5;
}
bool key_exists(HKEY root,const wchar_t* path){
    HKEY key{};LONG result=RegOpenKeyExW(root,path,0,KEY_READ|KEY_WOW64_64KEY,&key);
    if(result==ERROR_SUCCESS)RegCloseKey(key);
    return result==ERROR_SUCCESS;
}
void doctor_line(const wchar_t* label,bool ok,const std::wstring& detail){
    std::wcout<<(ok?L"  [PASS] ":L"  [WARN] ")<<label<<L": "<<detail<<L"\n";
}
int system_doctor(){
    const std::wstring root=exe_dir();ModInfo mod;std::wstring error;
    bool manifest=load_verinfo(root+L"\\verinfo.bin",mod,&error);
    WindowsInfo windows=get_windows_info();bool compatible=manifest&&build_supported(mod,windows);
    bool auth=path_exists(root+L"\\NebulaUserAuth.exe")&&path_exists(root+L"\\NebulaUserAuthService.exe");
    bool assets=path_exists(root+L"\\Assets\\Wallpapers");
    bool store=path_exists(root+L"\\N10Store.exe");
    bool localTools=true;for(const auto&tool:kLocalTools)localTools&=path_exists(root+L"\\"+tool.relative);
    bool service=false;SC_HANDLE manager=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_CONNECT);
    if(manager){SC_HANDLE handle=OpenServiceW(manager,kService,SERVICE_QUERY_STATUS);service=handle!=nullptr;if(handle)CloseServiceHandle(handle);CloseServiceHandle(manager);}
    bool reboot=key_exists(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Component Based Servicing\\RebootPending")||key_exists(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WindowsUpdate\\Auto Update\\RebootRequired");
    std::wcout<<L"Nebula System Doctor\n====================\n";
    doctor_line(L"verinfo.bin",manifest,manifest?mod.modVersion:error);
    doctor_line(L"Windows support",compatible,L"build "+windows.build+L" / declared "+mod.supportedBuilds);
    doctor_line(L"UserAuth",auth,auth?L"client and service binaries present":L"one or more binaries missing");
    doctor_line(L"Installed service",service,service?L"registered":L"not registered (normal in portable mode)");
    doctor_line(L"Assets",assets,assets?L"wallpaper pack available":L"wallpaper directory missing");
    doctor_line(L"N10Store",store,store?L"curated terminal store ready":L"N10Store.exe missing");
    doctor_line(L"Local tools",localTools,localTools?L"configured payload is complete":L"one or more local tool files are missing");
    doctor_line(L"Restart state",!reboot,reboot?L"Windows reports a pending restart":L"no common pending-restart marker");
    bool ready=manifest&&compatible&&auth&&assets&&store&&localTools;
    std::wcout<<L"Overall: "<<(ready?L"READY":L"ATTENTION NEEDED")<<L"\n";return 0;
}
int run_child_wait(const std::wstring& exe,const std::wstring& args=L""){
    std::wstring command=L"\""+exe+L"\""+(args.empty()?L"":L" "+args);std::vector<wchar_t> buf(command.begin(),command.end());buf.push_back(0);
    STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
    if(!CreateProcessW(exe.c_str(),buf.data(),nullptr,nullptr,FALSE,0,nullptr,exe_dir().c_str(),&si,&pi))return 5;
    WaitForSingleObject(pi.hProcess,INFINITE);DWORD code{};GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return static_cast<int>(code);
}
std::wstring quote_process_arg(const std::wstring& value){
    std::wstring quoted=L"\"";size_t backslashes=0;
    for(wchar_t ch:value){
        if(ch==L'\\'){++backslashes;continue;}
        if(ch==L'\"'){quoted.append(backslashes*2+1,L'\\');quoted.push_back(ch);backslashes=0;continue;}
        quoted.append(backslashes,L'\\');backslashes=0;quoted.push_back(ch);
    }
    quoted.append(backslashes*2,L'\\');quoted.push_back(L'\"');return quoted;
}
int run_child_wait_args(const std::wstring& exe,const std::vector<std::wstring>& args){
    std::wstring command=quote_process_arg(exe);for(const auto&arg:args)command+=L" "+quote_process_arg(arg);
    std::vector<wchar_t> buf(command.begin(),command.end());buf.push_back(0);
    STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
    if(!CreateProcessW(exe.c_str(),buf.data(),nullptr,nullptr,FALSE,0,nullptr,exe_dir().c_str(),&si,&pi)){std::wcerr<<L"Could not launch n10themes.exe (error "<<GetLastError()<<L").\n";return 5;}
    WaitForSingleObject(pi.hProcess,INFINITE);DWORD code{};GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return static_cast<int>(code);
}
bool valid_theme_pack_name(const std::wstring& name){
    if(name.empty()||name==L"."||name==L".."||name.back()==L'.'||name.back()==L' ')return false;
    for(wchar_t ch:name)if(ch<32||wcschr(L"/\\:<>\"|?*",ch))return false;
    return true;
}
int theme_updater_action(const std::wstring& action,bool dry){
    bool update=action==L"update",remove=action==L"daily-remove";
    if(!update&&action!=L"daily"&&!remove)return 2;
    std::wstring script=exe_dir()+L"\\ThemeUpdater\\"+(update?L"Update-N10Themes.ps1":L"Install-DailyUpdater.ps1");
    std::wstring label=update?L"update official themes now":remove?L"remove the daily theme update task":L"install or repair the daily theme update task";
    if(dry){std::wcout<<L"DRY-RUN: would "<<label<<L" using the fixed verified Nebula-10-Themes catalog.\n";return 0;}
    if(!require_nebula_integrity(L"n10toolbox.exe"))return 8;
    if(!path_exists(script)){std::wcerr<<L"Theme updater payload missing: "<<script<<L"\n";return 4;}
    const wchar_t* integrityName=update?L"ThemeUpdater\\Update-N10Themes.ps1":L"ThemeUpdater\\Install-DailyUpdater.ps1";
    if(!require_nebula_integrity(integrityName))return 8;
    wchar_t windows[MAX_PATH]{};GetWindowsDirectoryW(windows,MAX_PATH);std::wstring powershell=std::wstring(windows)+L"\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
    std::wstring arguments=L"-NoLogo -NoProfile -NonInteractive -ExecutionPolicy RemoteSigned -File "+quote_process_arg(script)+(remove?L" -Remove":L"");
    auto result=reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"runas",powershell.c_str(),arguments.c_str(),exe_dir().c_str(),SW_SHOWNORMAL));
    return result>32?0:5;
}
int theme_command(const std::vector<std::wstring>& args,bool dry){
    if(args.size()<2){std::wcerr<<L"themes requires help, roots, list, update, daily, daily-remove, select-wallpaper, or select-icons.\n";return 2;}
    const std::wstring& action=args[1];std::vector<std::wstring> child;
    if(action==L"help"&&args.size()==2)child.push_back(L"--help");
    else if((action==L"update"||action==L"daily"||action==L"daily-remove")&&args.size()==2)return theme_updater_action(action,dry);
    else if((action==L"roots"||action==L"list")&&args.size()==2)child.push_back(action);
    else if((action==L"select-wallpaper"||action==L"select-icons")&&args.size()==3){
        if(!valid_theme_pack_name(args[2])){std::wcerr<<L"Refused unsafe theme pack name.\n";return 2;}
        child={action,args[2]};if(dry)child.push_back(L"--dry-run");
    }else{std::wcerr<<L"Invalid themes command. Use --help.\n";return 2;}
    return run_child_wait_args(exe_dir()+L"\\n10themes.exe",child);
}

int request(const std::wstring& action,bool dry){
    if(!action_allowed(action)){std::wcerr<<L"Rejected: not an allowlisted action.\n";return 3;}
    std::wstring exe=exe_dir()+L"\\NebulaUserAuth.exe",cmd=L"\""+exe+L"\" "+action;
    if(dry){std::wcout<<L"DRY-RUN: would launch new authorization console for "<<action<<L"\n";return 0;}
    if(!require_nebula_integrity(L"n10toolbox.exe"))return 8;
    STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};std::vector<wchar_t> b(cmd.begin(),cmd.end());b.push_back(0);
    if(!CreateProcessW(exe.c_str(),b.data(),nullptr,nullptr,FALSE,CREATE_NEW_CONSOLE,nullptr,exe_dir().c_str(),&si,&pi)){
        std::wcerr<<L"Launch failed: "<<GetLastError()<<L"\n";return 4;
    }
    CloseHandle(pi.hThread);CloseHandle(pi.hProcess);std::wcout<<L"Nebula UserAuth console opened.\n";return 0;
}
int show_info(){
    MEMORYSTATUSEX m{};m.dwLength=sizeof(m);GlobalMemoryStatusEx(&m);wchar_t name[256]{};DWORD n=256;GetComputerNameW(name,&n);
    std::wcout<<L"Computer       : "<<name<<L"\nLogical CPUs   : "<<GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)
      <<L"\nPhysical memory: "<<(m.ullTotalPhys/(1024*1024))<<L" MiB\nMemory load    : "<<m.dwMemoryLoad<<L"%\n";return 0;
}
int diagnostics(const std::wstring& mode){
    if(mode==L"summary"){
        WindowsInfo w=get_windows_info();SYSTEM_INFO native{};GetNativeSystemInfo(&native);
        std::wstring arch=native.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64?L"x64":native.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_ARM64?L"ARM64":L"x86/other";
        std::wcout<<L"Diagnostics summary\n===================\n";
        show_info();
        std::wcout<<L"Windows        : "<<w.product<<L"\nBuild          : "<<w.build<<L"."<<w.ubr<<L"\nArchitecture   : "<<arch<<L"\n";
        return 0;
    }
    if(mode==L"storage"){
        wchar_t windows[MAX_PATH]{};GetWindowsDirectoryW(windows,MAX_PATH);std::wstring root=windows;root=root.substr(0,3);
        ULARGE_INTEGER available{},total{},free{};bool ok=GetDiskFreeSpaceExW(root.c_str(),&available,&total,&free)!=FALSE;
        std::wcout<<L"Storage diagnostics\n===================\nSystem volume: "<<root<<L"\n";
        if(ok){double divisor=1024.0*1024.0*1024.0;std::wcout<<std::fixed<<std::setprecision(1)<<L"Capacity     : "<<(total.QuadPart/divisor)<<L" GiB\nFree space   : "<<(free.QuadPart/divisor)<<L" GiB\nAvailable    : "<<(available.QuadPart/divisor)<<L" GiB\n";}
        else std::wcout<<L"Disk information unavailable (error "<<GetLastError()<<L").\n";
        return 0;
    }
    if(mode==L"battery"){
        SYSTEM_POWER_STATUS power{};bool ok=GetSystemPowerStatus(&power)!=FALSE;
        std::wcout<<L"Battery diagnostics\n===================\n";
        if(!ok)std::wcout<<L"Power information unavailable.\n";
        else if(power.BatteryFlag==128)std::wcout<<L"Battery       : Not present\nAC power      : "<<(power.ACLineStatus==1?L"Online":L"Offline or unknown")<<L"\n";
        else std::wcout<<L"Battery level : "<<(power.BatteryLifePercent==255?L"Unknown":std::to_wstring(power.BatteryLifePercent)+L"%")<<L"\nAC power      : "<<(power.ACLineStatus==1?L"Online":L"Offline")<<L"\n";
        return 0;
    }
    if(mode==L"display"){
        DEVMODEW display{};display.dmSize=sizeof(display);bool ok=EnumDisplaySettingsW(nullptr,ENUM_CURRENT_SETTINGS,&display)!=FALSE;
        std::wcout<<L"Display diagnostics\n===================\nMonitors       : "<<GetSystemMetrics(SM_CMONITORS)<<L"\nVirtual desktop: "<<GetSystemMetrics(SM_CXVIRTUALSCREEN)<<L" x "<<GetSystemMetrics(SM_CYVIRTUALSCREEN)<<L"\n";
        if(ok)std::wcout<<L"Primary mode  : "<<display.dmPelsWidth<<L" x "<<display.dmPelsHeight<<L" @ "<<display.dmDisplayFrequency<<L" Hz\nColor depth   : "<<display.dmBitsPerPel<<L" bpp\n";
        return 0;
    }
    std::wcerr<<L"Unknown diagnostics mode. Use summary, storage, battery, or display.\n";return 2;
}
int open_item(const std::wstring& label,const wchar_t* target,const wchar_t* args,bool dry){
    if(dry){std::wcout<<L"DRY-RUN: would open "<<label<<L" ("<<target<<L").\n";return 0;}
    return launch(target,args)?0:5;
}
int network_tool(const std::wstring& item,bool dry){
    if(item==L"list"){std::wcout<<L"Network tool catalog\n  connections  Network Connections\n  adapters     Device Manager\n  firewall     Advanced Firewall\n  proxy        Proxy settings\n  wifi         Wi-Fi settings\n";return 0;}
    if(item==L"connections")return open_item(L"Network Connections",L"ncpa.cpl",L"",dry);
    if(item==L"adapters")return open_item(L"Device Manager",L"devmgmt.msc",L"",dry);
    if(item==L"firewall")return open_item(L"Advanced Firewall",L"wf.msc",L"",dry);
    if(item==L"proxy")return open_item(L"Proxy settings",L"ms-settings:network-proxy",L"",dry);
    if(item==L"wifi")return open_item(L"Wi-Fi settings",L"ms-settings:network-wifi",L"",dry);
    return 2;
}
int windows_tool(const std::wstring& item,bool dry){
    if(item==L"list"){std::wcout<<L"Windows tool catalog\n  settings, taskmgr, terminal, control, events, services\n  devices, apps, dxdiag, resource, computer, diskmgmt\n  registry, sysinfo, reliability, performance, environment\n  features, security, power\n";return 0;}
    if(item==L"settings")return open_item(L"Settings",L"ms-settings:",L"",dry);
    if(item==L"taskmgr")return open_item(L"Task Manager",L"taskmgr.exe",L"",dry);
    if(item==L"terminal")return open_item(L"Command Prompt",L"cmd.exe",L"",dry);
    if(item==L"control")return open_item(L"Control Panel",L"control.exe",L"",dry);
    if(item==L"events")return open_item(L"Event Viewer",L"eventvwr.msc",L"",dry);
    if(item==L"services")return open_item(L"Services",L"services.msc",L"",dry);
    if(item==L"devices")return open_item(L"Device Manager",L"devmgmt.msc",L"",dry);
    if(item==L"apps")return open_item(L"Installed apps",L"ms-settings:appsfeatures",L"",dry);
    if(item==L"dxdiag")return open_item(L"DirectX Diagnostic Tool",L"dxdiag.exe",L"",dry);
    if(item==L"resource")return open_item(L"Resource Monitor",L"resmon.exe",L"",dry);
    if(item==L"computer")return open_item(L"Computer Management",L"compmgmt.msc",L"",dry);
    if(item==L"diskmgmt")return open_item(L"Disk Management",L"diskmgmt.msc",L"",dry);
    if(item==L"registry")return open_item(L"Registry Editor",L"regedit.exe",L"",dry);
    if(item==L"sysinfo")return open_item(L"System Information",L"msinfo32.exe",L"",dry);
    if(item==L"reliability")return open_item(L"Reliability Monitor",L"perfmon.exe",L"/rel",dry);
    if(item==L"performance")return open_item(L"Performance Monitor",L"perfmon.exe",L"",dry);
    if(item==L"environment")return open_item(L"Environment Variables",L"rundll32.exe",L"sysdm.cpl,EditEnvironmentVariables",dry);
    if(item==L"features")return open_item(L"Optional Features",L"optionalfeatures.exe",L"",dry);
    if(item==L"security")return open_item(L"Windows Security",L"windowsdefender:",L"",dry);
    if(item==L"power")return open_item(L"Power Options",L"powercfg.cpl",L"",dry);
    return 2;
}
int maintenance_tool(const std::wstring& item,bool dry){
    if(item==L"list"){std::wcout<<L"Maintenance catalog\n  diskcleanup  Disk Cleanup\n  optimize     Optimize Drives\n  storage      Storage settings\n  update       Windows Update\n  recovery     System Protection\n  startup      Startup Apps\n  troubleshoot Troubleshooters\n  backup       Backup settings\n  restore      System Restore\n";return 0;}
    if(item==L"diskcleanup")return open_item(L"Disk Cleanup",L"cleanmgr.exe",L"",dry);
    if(item==L"optimize")return open_item(L"Optimize Drives",L"dfrgui.exe",L"",dry);
    if(item==L"storage")return open_item(L"Storage settings",L"ms-settings:storagesense",L"",dry);
    if(item==L"update")return open_item(L"Windows Update",L"ms-settings:windowsupdate",L"",dry);
    if(item==L"recovery")return open_item(L"System Protection",L"SystemPropertiesProtection.exe",L"",dry);
    if(item==L"startup")return open_item(L"Startup Apps",L"ms-settings:startupapps",L"",dry);
    if(item==L"troubleshoot")return open_item(L"Troubleshooters",L"ms-settings:troubleshoot",L"",dry);
    if(item==L"backup")return open_item(L"Backup Settings",L"ms-settings:backup",L"",dry);
    if(item==L"restore")return open_item(L"System Restore",L"rstrui.exe",L"",dry);
    return 2;
}
int open_nebula_folder(const std::wstring& kind,bool dry){
    std::wstring path;
    if(kind==L"assets")path=exe_dir()+L"\\Assets";
    else{wchar_t data[MAX_PATH]{};GetEnvironmentVariableW(L"ProgramData",data,MAX_PATH);path=std::wstring(data)+L"\\Nebula10";}
    std::wstring label=kind==L"assets"?L"Assets":L"Logs";
    if(dry){std::wcout<<L"DRY-RUN: would open Nebula "<<label<<L" folder: "<<path<<L"\n";return 0;}
    if(!path_exists(path)){std::wcerr<<label<<L" folder does not exist: "<<path<<L"\n";return 2;}
    return launch(path.c_str())?0:5;
}
int apply_profile(const std::wstring& profile,bool dry){
    if(dry){std::wcout<<L"DRY-RUN: would back up and set conservative HKCU "<<profile<<L" values.\n";return 0;}
    if(!require_nebula_integrity(L"n10toolbox.exe"))return 8;
    bool ok=true;
    if(profile==L"privacy"){
        ok&=set_hkcu_dword_managed(L"AdId",L"Software\\Microsoft\\Windows\\CurrentVersion\\AdvertisingInfo",L"Enabled",0);
        ok&=set_hkcu_dword_managed(L"Tailored",L"Software\\Microsoft\\Windows\\CurrentVersion\\Privacy",L"TailoredExperiencesWithDiagnosticDataEnabled",0);
        ok&=set_hkcu_dword_managed(L"Suggestions",L"Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",L"SubscribedContent-338389Enabled",0);
    }else if(profile==L"balanced"){
        ok&=set_hkcu_dword_managed(L"Transparency",L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"EnableTransparency",0);
        ok&=set_hkcu_dword_managed(L"TaskbarAnimations",L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",L"TaskbarAnimations",0);
        ok&=set_hkcu_string_managed(L"MenuDelay",L"Control Panel\\Desktop",L"MenuShowDelay",L"150");
    }else if(profile==L"poweruser"){
        ok&=set_hkcu_dword_managed(L"FileExtensions",L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",L"HideFileExt",0);
        ok&=set_hkcu_dword_managed(L"HiddenFiles",L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",L"Hidden",1);
    }else{
        std::wcerr<<L"Unknown profile.\n";return 2;
    }
    std::wcout<<(ok?L"Applied managed settings; original values are saved.\n":L"A setting failed.\n");return ok?0:5;
}
int rollback(bool dry){
    if(dry){std::wcout<<L"DRY-RUN: would restore every Nebula10-managed HKCU value.\n";return 0;}
    if(!require_nebula_integrity(L"n10toolbox.exe"))return 8;
    bool ok=true;
    ok&=restore_hkcu_managed(L"AdId",L"Software\\Microsoft\\Windows\\CurrentVersion\\AdvertisingInfo",L"Enabled");
    ok&=restore_hkcu_managed(L"Tailored",L"Software\\Microsoft\\Windows\\CurrentVersion\\Privacy",L"TailoredExperiencesWithDiagnosticDataEnabled");
    ok&=restore_hkcu_managed(L"Suggestions",L"Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",L"SubscribedContent-338389Enabled");
    ok&=restore_hkcu_managed(L"Transparency",L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"EnableTransparency");
    ok&=restore_hkcu_managed(L"TaskbarAnimations",L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",L"TaskbarAnimations");
    ok&=restore_hkcu_managed(L"MenuDelay",L"Control Panel\\Desktop",L"MenuShowDelay");
    ok&=restore_hkcu_managed(L"FileExtensions",L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",L"HideFileExt");
    ok&=restore_hkcu_managed(L"HiddenFiles",L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",L"Hidden");
    std::wcout<<(ok?L"Nebula10 per-user settings restored.\n":L"One or more values could not be restored.\n");return ok?0:5;
}
int wallpaper(const std::wstring& path,bool dry){
    if(dry){std::wcout<<L"DRY-RUN: would select wallpaper: "<<path<<L"\n";return 0;}
    if(!require_nebula_integrity(L"n10toolbox.exe"))return 8;
    DWORD a=GetFileAttributesW(path.c_str());if(a==INVALID_FILE_ATTRIBUTES||(a&FILE_ATTRIBUTE_DIRECTORY)){std::wcerr<<L"Wallpaper file not found.\n";return 2;}
    return SystemParametersInfoW(SPI_SETDESKWALLPAPER,0,const_cast<wchar_t*>(path.c_str()),SPIF_UPDATEINIFILE|SPIF_SENDCHANGE)?0:5;
}
int dispatch(const std::vector<std::wstring>& args,bool dry){
    if(args.empty())return 2;
    const auto& c=args[0];
    if(c==L"info")return show_info();
    if(c==L"doctor")return system_doctor();
    if(c==L"privacy"||c==L"balanced"||c==L"poweruser")return apply_profile(c,dry);
    if(c==L"diagnostics"&&args.size()>=2)return diagnostics(args[1]);
    if(c==L"rollback")return rollback(dry);
    if(c==L"wallpaper"){if(args.size()<2){std::wcerr<<L"wallpaper requires a file.\n";return 2;}return wallpaper(args[1],dry);}
    if(c==L"request"){if(args.size()<2)return 2;return request(args[1],dry);}
    if(c==L"network"&&args.size()>=2)return network_tool(args[1],dry);
    if(c==L"tool"&&args.size()>=2)return windows_tool(args[1],dry);
    if(c==L"maintenance"&&args.size()>=2)return maintenance_tool(args[1],dry);
    if(c==L"localtool")return local_tool_command(args,dry);
    if(c==L"themes")return theme_command(args,dry);
    if(c==L"assets"||c==L"logs")return open_nebula_folder(c,dry);
    std::wcerr<<L"Unknown or incomplete command. Use --help.\n";return 2;
}
struct MenuEntry{std::wstring primary;std::wstring secondary;};
bool console_input(){DWORD mode{};return GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE),&mode)!=FALSE;}
std::wstring toolbox_build_id(){ModInfo mod;std::wstring error;return load_verinfo(exe_dir()+L"\\verinfo.bin",mod,&error)?mod.buildId:L"N10-UNKNOWN";}
void draw_menu(const std::wstring& title,const std::vector<MenuEntry>& entries,size_t selected,bool dry,bool logo){
    clear_screen();
    if(logo)std::wcout<<L"\x1b[96m"
      L"    _   ____________  __  ____    ___ \n"
      L"   / | / / ____/ __ )/ / / / /   /   |\n"
      L"  /  |/ / __/ / __  / / / / /   / /| |\n"
      L" / /|  / /___/ /_/ / /_/ / /___/ ___ |\n"
      L"/_/ |_/_____/_____/\\____/_____/_/  |_|\n\x1b[0m"
      L"                       NEBULA TOOLBOX\n";
    std::wcout<<L"  __________________________________________________________________________\n\n"
      L"              | Nebula10 | "<<toolbox_build_id()<<L" | ToolBox v2.1 |\n\n"
      L"  "<<title<<L"\n\n";
    const size_t visibleRows=logo?10:14;
    size_t start=selected>=visibleRows?selected-visibleRows+1:0;
    if(entries.size()>visibleRows&&start+visibleRows>entries.size())start=entries.size()-visibleRows;
    size_t end=std::min(entries.size(),start+visibleRows);
    if(start)std::wcout<<L"    \x1b[90m... more above ...\x1b[0m\n";
    for(size_t i=start;i<end;++i){
        if(i==selected)std::wcout<<L"\x1b[92m  > "<<entries[i].primary<<L"\x1b[0m";
        else std::wcout<<L"    "<<entries[i].primary;
        if(!entries[i].secondary.empty())std::wcout<<L"  \x1b[90m["<<entries[i].secondary<<L"]\x1b[0m";
        std::wcout<<L"\n";
    }
    if(end<entries.size())std::wcout<<L"    \x1b[90m... more below ...\x1b[0m\n";
    if(entries.size()>visibleRows)std::wcout<<L"\n  Showing "<<(start+1)<<L"-"<<end<<L" of "<<entries.size()<<L"\n";
    std::wcout<<L"\n  __________________________________________________________________________\n\n"
      L"  UP/DOWN or W/S navigate   ENTER select   ESC back   "<<(dry?L"DRY-RUN":L"LIVE MODE")<<L"\n";
}
int select_menu(const std::wstring& title,const std::vector<MenuEntry>& entries,bool dry,bool logo=false){
    size_t selected=0;bool interactive=console_input();
    for(;;){
        draw_menu(title,entries,selected,dry,logo);
        if(!interactive){
            std::wcout<<L"  Select: ";std::wstring input;if(!std::getline(std::wcin,input))return -1;
            if(input==L"0")return static_cast<int>(entries.size()-1);
            try{size_t value=std::stoul(input);if(value>=1&&value<=entries.size())return static_cast<int>(value-1);}catch(...){}
            std::wcout<<L"Invalid selection.\n";return -1;
        }
        int key=_getwch();
        if(key==0||key==224){int scan=_getwch();if(scan==72)selected=selected?selected-1:entries.size()-1;else if(scan==80)selected=(selected+1)%entries.size();continue;}
        if(key==L'w'||key==L'W')selected=selected?selected-1:entries.size()-1;
        else if(key==L's'||key==L'S')selected=(selected+1)%entries.size();
        else if(key==13)return static_cast<int>(selected);
        else if(key==27)return -1;
    }
}
void pause_action(){
    std::wcout<<L"\n  Press any key to return...";
    if(console_input())_getwch();else{std::wstring ignored;std::getline(std::wcin,ignored);}
}
void action_menu(const std::wstring& title,const std::vector<MenuEntry>& entries,bool dry,const std::function<int(size_t)>& action){
    for(;;){int selected=select_menu(title,entries,dry);if(selected<0||static_cast<size_t>(selected)==entries.size()-1)return;clear_screen();int rc=action(static_cast<size_t>(selected));if(rc)std::wcout<<L"\nAction returned code "<<rc<<L".\n";pause_action();}
}
void configure_local_tools(bool dry){
    for(;;){std::vector<MenuEntry> entries;for(const auto&tool:kLocalTools)entries.push_back({tool.name,local_tool_enabled(tool)?L"Enabled - select to disable":L"Disabled - select to enable"});entries.push_back({L"Back",L"Return to Local Nebula Tools"});int selected=select_menu(L"Configure Local Tools",entries,dry);if(selected<0||static_cast<size_t>(selected)==std::size(kLocalTools))return;const auto&tool=kLocalTools[selected];bool enabled=!local_tool_enabled(tool);clear_screen();if(dry)std::wcout<<L"DRY-RUN: would set "<<tool.name<<L" to "<<(enabled?L"Enabled":L"Disabled")<<L".\n";else std::wcout<<(set_local_tool_enabled(tool,enabled)?L"Updated ":L"Could not update ")<<tool.name<<L".\n";pause_action();}
}
void local_tools_tui(bool dry){
    for(;;){std::vector<const LocalTool*> visible;std::vector<MenuEntry> entries;for(const auto&tool:kLocalTools)if(local_tool_enabled(tool)){visible.push_back(&tool);entries.push_back({tool.name,tool.description});}entries.push_back({L"Configure Tools",L"Enable or disable supplied tools"});entries.push_back({L"Back",L"Return to Main Menu"});int selected=select_menu(L"Local Nebula Tools",entries,dry);if(selected<0||static_cast<size_t>(selected)==entries.size()-1)return;if(static_cast<size_t>(selected)==visible.size()){configure_local_tools(dry);continue;}const auto*tool=visible[selected];clear_screen();int rc=local_tool_command({L"localtool",L"launch",tool->id},dry);if(rc)std::wcout<<L"\nAction returned code "<<rc<<L".\n";pause_action();}
}
std::wstring environment_path(const wchar_t* name){
    DWORD needed=GetEnvironmentVariableW(name,nullptr,0);if(!needed)return L"";
    std::vector<wchar_t> value(needed);return GetEnvironmentVariableW(name,value.data(),needed)?value.data():L"";
}
std::wstring official_theme_root(){std::wstring overridePath=environment_path(L"NEBULA_THEME_ROOT");return overridePath.empty()?L"C:\\Windows\\NebulaData\\Themes":overridePath;}
std::wstring selected_theme_root(){
    std::wstring overridePath=environment_path(L"NEBULA_THEME_DOCUMENTS");if(!overridePath.empty())return overridePath;
    wchar_t documents[MAX_PATH]{};return SUCCEEDED(SHGetFolderPathW(nullptr,CSIDL_PERSONAL,nullptr,SHGFP_TYPE_CURRENT,documents))?std::wstring(documents)+L"\\Themes":L"Documents\\Themes";
}
std::vector<std::wstring> theme_packs(const wchar_t* category){
    std::vector<std::wstring> packs;WIN32_FIND_DATAW data{};std::wstring pattern=official_theme_root()+L"\\"+category+L"\\*";HANDLE find=FindFirstFileW(pattern.c_str(),&data);
    if(find==INVALID_HANDLE_VALUE)return packs;
    do{std::wstring name=data.cFileName;if((data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&!(data.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)&&name!=L"."&&name!=L".."&&valid_theme_pack_name(name))packs.push_back(name);}while(FindNextFileW(find,&data));
    FindClose(find);std::sort(packs.begin(),packs.end(),[](const std::wstring&a,const std::wstring&b){return _wcsicmp(a.c_str(),b.c_str())<0;});return packs;
}
int open_theme_folder(bool official,bool dry){
    std::wstring path=official?official_theme_root():selected_theme_root();const wchar_t* label=official?L"official theme storage":L"selected themes";
    if(dry){std::wcout<<L"DRY-RUN: would open "<<label<<L": "<<path<<L"\n";return 0;}
    if(!path_exists(path)){std::wcerr<<L"Theme folder does not exist: "<<path<<L"\n";return 2;}
    return launch(path.c_str())?0:5;
}
void select_theme_pack_tui(bool wallpaperPack,bool dry){
    const wchar_t* category=wallpaperPack?L"Wallpapers":L"Icons";std::vector<std::wstring> packs=theme_packs(category);
    if(packs.empty()){clear_screen();std::wcout<<L"No official "<<category<<L" packs are currently installed.\n\n";theme_command({L"themes",L"list"},dry);pause_action();return;}
    std::vector<MenuEntry> entries;for(const auto&pack:packs)entries.push_back({pack,wallpaperPack?L"Copy and apply this wallpaper pack":L"Copy this icon pack"});entries.push_back({L"Back",L"Return to N10 Themes"});
    for(;;){int selected=select_menu(wallpaperPack?L"Select Wallpaper Pack":L"Select Icon Pack",entries,dry);if(selected<0||static_cast<size_t>(selected)==packs.size())return;clear_screen();int rc=theme_command({L"themes",wallpaperPack?L"select-wallpaper":L"select-icons",packs[selected]},dry);if(rc)std::wcout<<L"\nAction returned code "<<rc<<L".\n";pause_action();}
}
void themes_tui(bool dry){
    const std::vector<MenuEntry> entries={
      {L"Official Pack Catalog",L"Show installed wallpaper and icon packs"},
      {L"Update Official Themes Now",L"Fetch fixed JSON catalog and verify SHA-256 hashes"},
      {L"Install Daily Updates",L"Run verified catalog sync every day at 05:23"},
      {L"Remove Daily Updates",L"Unregister the automatic catalog task"},
      {L"Select Wallpaper Pack",L"Choose an official wallpaper pack"},
      {L"Select Icon Pack",L"Choose an official icon pack"},
      {L"Open Official Storage",L"C:\\Windows\\NebulaData\\Themes"},
      {L"Open Selected Themes",L"Documents\\Themes"},
      {L"Back",L"Return to Main Menu"}};
    for(;;){int selected=select_menu(L"N10 Themes",entries,dry);if(selected<0||selected==8)return;if(selected==4){select_theme_pack_tui(true,dry);continue;}if(selected==5){select_theme_pack_tui(false,dry);continue;}clear_screen();int rc=selected==0?theme_command({L"themes",L"list"},dry):selected==1?theme_command({L"themes",L"update"},dry):selected==2?theme_command({L"themes",L"daily"},dry):selected==3?theme_command({L"themes",L"daily-remove"},dry):open_theme_folder(selected==6,dry);if(rc)std::wcout<<L"\nAction returned code "<<rc<<L".\n";pause_action();}
}
int run_tui(bool dry){
    enable_console_ui();
    const std::vector<MenuEntry> main={
      {L"System & Identity",L"Dashboard, N10 version, and health"},
      {L"Customize & Tune",L"Reversible user profiles and wallpapers"},
      {L"Diagnostics",L"System, storage, battery, and display"},
      {L"Network Center",L"Connections, adapters, firewall, proxy, Wi-Fi"},
      {L"Windows Tools",L"Settings, consoles, devices, apps, and logs"},
      {L"Maintenance",L"Cleanup, drives, updates, and recovery"},
      {L"Machine Features",L"Narrow UserAuth service actions"},
      {L"Local Nebula Tools",L"Configurable tools supplied in the Nebula tools folder"},
      {L"N10 Themes",L"Official wallpaper and icon pack catalog"},
      {L"N10Store",L"Curated software store using bundled Chocolatey"},
      {L"Recovery & Files",L"Rollback, Nebula logs, assets, and help"},
      {L"Exit",L"Close Nebula ToolBox"},
    };
    for(;;){
        int selected=select_menu(L"Main Menu",main,dry,true);
        if(selected<0||selected==11){std::wcout<<L"Goodbye from Nebula ToolBox.\n";return 0;}
        if(selected==0)action_menu(L"System & Identity",{{L"System Dashboard",L"Live hardware summary"},{L"N10 Version",L"Nebula and genuine Windows identity"},{L"System Doctor",L"Installation and readiness checks"},{L"Back",L"Return to Main Menu"}},dry,[&](size_t i){if(i==0)return show_info();if(i==1)return run_child_wait(exe_dir()+L"\\n10ver.exe");return system_doctor();});
        else if(selected==1)action_menu(L"Customize & Tune",{{L"Privacy Profile",L"Conservative current-user privacy"},{L"Balanced Profile",L"Conservative visual responsiveness"},{L"Power User Profile",L"Show extensions and hidden files"},{L"Wallpaper: Aurora",L"Nebula artwork"},{L"Wallpaper: Midnight",L"Nebula artwork"},{L"Wallpaper: Violet",L"Nebula artwork"},{L"Back",L"Return to Main Menu"}},dry,[&](size_t i){if(i<3)return apply_profile(i==0?L"privacy":i==1?L"balanced":L"poweruser",dry);const wchar_t* name=i==3?L"Aurora":i==4?L"Midnight":L"Violet";return wallpaper(exe_dir()+L"\\Assets\\Wallpapers\\Nebula-"+name+L".png",dry);});
        else if(selected==2)action_menu(L"Diagnostics",{{L"Summary",L"Windows, CPU, and memory"},{L"Storage",L"System-volume capacity and free space"},{L"Battery",L"Power source and charge"},{L"Display",L"Monitor and primary display mode"},{L"Back",L"Return to Main Menu"}},dry,[&](size_t i){const wchar_t* modes[]={L"summary",L"storage",L"battery",L"display"};return diagnostics(modes[i]);});
        else if(selected==3)action_menu(L"Network Center",{{L"Network Connections",L"Classic adapter panel"},{L"Device Adapters",L"Device Manager"},{L"Advanced Firewall",L"Microsoft firewall console"},{L"Proxy Settings",L"Windows proxy page"},{L"Wi-Fi Settings",L"Windows Wi-Fi page"},{L"Back",L"Return to Main Menu"}},dry,[&](size_t i){const wchar_t* items[]={L"connections",L"adapters",L"firewall",L"proxy",L"wifi"};return network_tool(items[i],dry);});
        else if(selected==4)action_menu(L"Windows Tools",{{L"Settings",L"Windows Settings"},{L"Task Manager",L"Processes and performance"},{L"Command Prompt",L"Standard user terminal"},{L"Control Panel",L"Classic settings"},{L"Event Viewer",L"System event logs"},{L"Services",L"Service management console"},{L"Device Manager",L"Hardware devices"},{L"Installed Apps",L"Apps and features"},{L"DirectX Diagnostics",L"Graphics and audio details"},{L"Resource Monitor",L"Detailed resource activity"},{L"Computer Management",L"Disks, users, tasks, and events"},{L"Disk Management",L"Volumes and partitions"},{L"Registry Editor",L"Advanced registry editor"},{L"System Information",L"Detailed hardware and OS report"},{L"Reliability Monitor",L"Stability history and failures"},{L"Performance Monitor",L"Counters and data collector sets"},{L"Environment Variables",L"User and system environment"},{L"Optional Features",L"Windows optional components"},{L"Windows Security",L"Microsoft security dashboard"},{L"Power Options",L"Power plans and sleep"},{L"Back",L"Return to Main Menu"}},dry,[&](size_t i){const wchar_t* items[]={L"settings",L"taskmgr",L"terminal",L"control",L"events",L"services",L"devices",L"apps",L"dxdiag",L"resource",L"computer",L"diskmgmt",L"registry",L"sysinfo",L"reliability",L"performance",L"environment",L"features",L"security",L"power"};return windows_tool(items[i],dry);});
        else if(selected==5)action_menu(L"Maintenance",{{L"Disk Cleanup",L"Microsoft cleanup utility"},{L"Optimize Drives",L"Microsoft drive optimizer"},{L"Storage Settings",L"Storage Sense and usage"},{L"Windows Update",L"Microsoft update page"},{L"System Protection",L"Restore points and recovery"},{L"Startup Apps",L"Programs that run at sign-in"},{L"Troubleshooters",L"Recommended and additional troubleshooters"},{L"Backup Settings",L"Windows backup configuration"},{L"System Restore",L"Launch restore-point recovery"},{L"Back",L"Return to Main Menu"}},dry,[&](size_t i){const wchar_t* items[]={L"diskcleanup",L"optimize",L"storage",L"update",L"recovery",L"startup",L"troubleshoot",L"backup",L"restore"};return maintenance_tool(items[i],dry);});
        else if(selected==6)action_menu(L"Machine Features",{{L"Enable Long Paths",L"Allowlisted machine action"},{L"Restore Long Paths",L"Restore captured original state"},{L"Apply OEM Branding",L"Allowlisted Nebula branding"},{L"Restore OEM Branding",L"Restore captured original state"},{L"Back",L"Return to Main Menu"}},dry,[&](size_t i){const wchar_t* actions[]={L"LONG_PATHS_ON",L"LONG_PATHS_OFF",L"OEM_BRANDING_ON",L"OEM_BRANDING_OFF"};return request(actions[i],dry);});
        else if(selected==7)local_tools_tui(dry);
        else if(selected==8)themes_tui(dry);
        else if(selected==9){run_child_wait(exe_dir()+L"\\N10Store.exe",dry?L"--dry-run":L"");}
        else if(selected==10)action_menu(L"Recovery & Files",{{L"Rollback User Settings",L"Restore all captured HKCU values"},{L"Open Nebula Logs",L"ProgramData service logs"},{L"Open Nebula Assets",L"Wallpapers and branding"},{L"Command Help",L"All automation commands"},{L"Back",L"Return to Main Menu"}},dry,[&](size_t i){if(i==0)return rollback(dry);if(i==1)return open_nebula_folder(L"logs",dry);if(i==2)return open_nebula_folder(L"assets",dry);help();return 0;});
    }
}
}

int wmain(int argc,wchar_t** argv){
    bool dry=nebula::has_arg(argc,argv,L"--dry-run");
    if(nebula::has_arg(argc,argv,L"--help")||nebula::has_arg(argc,argv,L"-h")){help();return 0;}
    if(argc<2||_wcsicmp(argv[1],L"menu")==0)return run_tui(dry);
    std::vector<std::wstring> args;for(int i=1;i<argc;i++)if(_wcsicmp(argv[i],L"--dry-run")!=0)args.push_back(argv[i]);
    return dispatch(args,dry);
}
