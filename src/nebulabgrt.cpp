#include "common.hpp"
#include <fstream>
#include <shellapi.h>
#include <vector>

namespace {
using namespace nebula;
#pragma pack(push,1)
struct BmpHeader { unsigned short signature; unsigned int fileSize; unsigned int reserved; unsigned int dataOffset; unsigned int headerSize; int width; int height; unsigned short planes; unsigned short bits; };
#pragma pack(pop)
void help(){
    std::wcout<<L"NebulaBGRT - command-driven Nebula boot-logo controller\n"
      L"Usage:\n"
      L"  NebulaBGRT -status\n"
      L"  NebulaBGRT -safety-status\n"
      L"  NebulaBGRT -install [--dry-run] [--yes]\n"
      L"  NebulaBGRT -disable [--dry-run] [--yes]\n"
      L"  NebulaBGRT -uninstall [--dry-run] [--yes]\n"
      L"  NebulaBGRT -logo BMP_FILE [--dry-run]\n"
      L"No upstream setup menu is shown; operations use batch command input.\n";
}
std::wstring runtime_dir(){return exe_dir()+L"\\NebulaBGRT\\Runtime";}
std::wstring engine_path(){return runtime_dir()+L"\\engine.exe";}
bool exists(const std::wstring& p){DWORD a=GetFileAttributesW(p.c_str());return a!=INVALID_FILE_ATTRIBUTES&&!(a&FILE_ATTRIBUTE_DIRECTORY);}
bool is_admin(){BOOL admin=FALSE;PSID sid{};SID_IDENTIFIER_AUTHORITY authority=SECURITY_NT_AUTHORITY;if(AllocateAndInitializeSid(&authority,2,SECURITY_BUILTIN_DOMAIN_RID,DOMAIN_ALIAS_RID_ADMINS,0,0,0,0,0,0,&sid)){CheckTokenMembership(nullptr,sid,&admin);FreeSid(sid);}return admin==TRUE;}
int elevate_install_controller(bool yes){
    std::wstring parameters=L"-install --safety-elevated"+(yes?std::wstring(L" --yes"):std::wstring());SHELLEXECUTEINFOW launch{};launch.cbSize=sizeof(launch);launch.fMask=SEE_MASK_NOCLOSEPROCESS;launch.lpVerb=L"runas";std::wstring self=exe_dir()+L"\\NebulaBGRT.exe";launch.lpFile=self.c_str();launch.lpParameters=parameters.c_str();launch.lpDirectory=exe_dir().c_str();launch.nShow=SW_SHOWNORMAL;if(!ShellExecuteExW(&launch)||!launch.hProcess){std::wcerr<<L"BitLocker safety elevation was cancelled or failed.\n";return 5;}WaitForSingleObject(launch.hProcess,INFINITE);DWORD code{};GetExitCodeProcess(launch.hProcess,&code);CloseHandle(launch.hProcess);return static_cast<int>(code);
}
int status(){
    std::wstring rt=runtime_dir();FIRMWARE_TYPE firmware=FirmwareTypeUnknown;GetFirmwareType(&firmware);
    DWORD secure=reg_dword(HKEY_LOCAL_MACHINE,L"SYSTEM\\CurrentControlSet\\Control\\SecureBoot\\State",L"UEFISecureBootEnabled",0);
    bool engine=exists(engine_path()),loader=exists(rt+L"\\efi-signed\\bootx64.efi"),logo=exists(rt+L"\\splash.bmp");
    std::wcout<<L"NebulaBGRT status\n"
      L"  Firmware : "<<(firmware==FirmwareTypeUefi?L"UEFI":firmware==FirmwareTypeBios?L"Legacy BIOS":L"Unknown")<<L"\n"
      L"  Secure Boot: "<<(secure?L"Enabled":L"Disabled or unavailable")<<L"\n"
      L"  Payload engine : "<<(engine?L"Ready":L"Missing")<<L"\n"
      L"  Payload x64 EFI: "<<(loader?L"Ready":L"Missing")<<L"\n"
      L"  Payload logo   : "<<(logo?L"Ready":L"Missing")<<L"\n";
    return 0;
}
int run_engine(const std::wstring& args){
    std::wstring engine=engine_path();if(!exists(engine)){std::wcerr<<L"NebulaBGRT runtime engine is missing: "<<engine<<L"\n";return 2;}
    std::wstring cmd=L"\""+engine+L"\" "+args;std::vector<wchar_t> b(cmd.begin(),cmd.end());b.push_back(0);
    STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
    if(!CreateProcessW(engine.c_str(),b.data(),nullptr,nullptr,FALSE,0,nullptr,runtime_dir().c_str(),&si,&pi)){
        std::wcerr<<L"Unable to start NebulaBGRT engine: "<<GetLastError()<<L"\n";return 5;
    }
    WaitForSingleObject(pi.hProcess,INFINITE);DWORD code{};GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return static_cast<int>(code);
}
int set_logo(const std::wstring& source,bool dry){
    std::ifstream f(source.c_str(),std::ios::binary);if(!f){std::wcerr<<L"Logo file not found.\n";return 2;}
    BmpHeader h{};f.read(reinterpret_cast<char*>(&h),sizeof(h));
    if(!f||h.signature!=0x4D42||h.bits!=24||h.width<=0||h.height==0){std::wcerr<<L"Logo must be a valid 24-bit BMP.\n";return 3;}
    std::wstring target=runtime_dir()+L"\\splash.bmp";
    if(dry){std::wcout<<L"DRY-RUN: would install "<<h.width<<L"x"<<abs(h.height)<<L" 24-bit logo at "<<target<<L"\n";return 0;}
    CreateDirectoryW((exe_dir()+L"\\NebulaBGRT").c_str(),nullptr);CreateDirectoryW(runtime_dir().c_str(),nullptr);
    if(exists(target))CopyFileW(target.c_str(),(target+L".previous").c_str(),FALSE);
    if(!CopyFileW(source.c_str(),target.c_str(),FALSE)){std::wcerr<<L"Logo copy failed: "<<GetLastError()<<L"\n";return 5;}
    std::wcout<<L"NebulaBGRT logo updated. Run NebulaBGRT -install to apply it.\n";return 0;
}
bool confirmed(const std::wstring& action,bool yes,bool dry){
    if(yes||dry)return true;
    std::wcout<<L"WARNING: "<<action<<L" changes the UEFI boot chain and can trigger BitLocker/TPM recovery.\n"
      L"Keep recovery media and your BitLocker recovery key available.\nType "<<action<<L" to continue: ";
    std::wstring answer;std::getline(std::wcin,answer);return answer==action;
}
}

int wmain(int argc,wchar_t** argv){
    if(argc<2||nebula::has_arg(argc,argv,L"--help")||nebula::has_arg(argc,argv,L"-h")){help();return 0;}
    bool dry=nebula::has_arg(argc,argv,L"--dry-run"),yes=nebula::has_arg(argc,argv,L"--yes"),safetyElevated=nebula::has_arg(argc,argv,L"--safety-elevated");std::wstring command=argv[1];
    if(command==L"-status"||command==L"status")return status();
    if(command==L"-safety-status"||command==L"safety-status"){
        auto protection=nebula::query_system_drive_bitlocker();
        std::wcout<<L"BitLocker safety status: "<<(protection==nebula::BitLockerProtection::ProtectionOn?L"Protection On":protection==nebula::BitLockerProtection::ProtectionOff?L"Protection Off":L"Unknown (boot mutation blocked)")<<L"\n";
        return protection==nebula::BitLockerProtection::ProtectionOn?9:protection==nebula::BitLockerProtection::ProtectionOff?0:10;
    }
    if(command==L"-logo"||command==L"logo"){if(argc<3){std::wcerr<<L"-logo requires a BMP file.\n";return 2;}return set_logo(argv[2],dry);}
    std::wstring action,args=L"batch ";
    if(dry)args+=L"dry-run ";
    if(command==L"-install"||command==L"install"){
        action=L"INSTALL";
        args+=dry?L"install":L"disable install enable-bcdedit";
    }
    else if(command==L"-disable"||command==L"disable"){
        action=L"DISABLE";
        if(dry){std::wcout<<L"DRY-RUN: would disable the NebulaBGRT boot entry and restore the Windows boot path.\n";return 0;}
        args+=L"disable";
    }
    else if(command==L"-uninstall"||command==L"uninstall"){
        action=L"UNINSTALL";
        if(dry){std::wcout<<L"DRY-RUN: would disable NebulaBGRT and remove its EFI files.\n";return 0;}
        args+=L"uninstall";
    }
    else{std::wcerr<<L"Unknown NebulaBGRT command. Use --help.\n";return 2;}
    if(!dry&&!nebula::require_nebula_integrity(L"NebulaBGRT.exe"))return 8;
    if(action==L"INSTALL"&&!dry){
        auto protection=nebula::query_system_drive_bitlocker();
        if(protection==nebula::BitLockerProtection::ProtectionOn){std::wcerr<<L"Blocked before EFI mutation: BitLocker protection is enabled. Suspend it only with your recovery key available, then retry deliberately.\n";return 9;}
        if(protection==nebula::BitLockerProtection::Unknown){if(!safetyElevated&&!is_admin())return elevate_install_controller(yes);std::wcerr<<L"Blocked before EFI mutation: BitLocker status could not be verified safely.\n";return 10;}
    }
    if(!confirmed(action,yes,dry)){std::wcout<<L"Cancelled.\n";return 1;}
    return run_engine(args);
}
