#include "common.hpp"
#include <shellapi.h>
#include <sddl.h>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs=std::filesystem;
namespace {

void help(){
    std::wcout<<L"Nebula ForceOwn - explicit ownership and ACL repair\n"
      L"Usage:\n"
      L"  n10forceown [options] [--] [PATH ...]\n\n"
      L"With no PATH, the current directory is selected. Directories are recursive by default.\n"
      L"Multiple PATH arguments are supported for Explorer multi-selection.\n\n"
      L"Options:\n"
      L"  --dry-run         Preview targets and fixed native actions only\n"
      L"  --no-recursive    Change only the selected directory, not its contents\n"
      L"  --allow-system    Permit protected roots (requires stronger confirmation)\n"
      L"  --yes             Confirm non-interactively (still requires --allow-system)\n"
      L"  --shell           Explorer mode; pause before closing\n"
      L"  --help            Show this help\n\n"
      L"ForceOwn uses Windows takeown.exe and icacls.exe only. It never executes a supplied command.\n";
}

std::wstring quote(const std::wstring&s){
    if(s.empty())return L"\"\"";
    std::wstring out=L"\"";unsigned slashes=0;
    for(wchar_t c:s){
        if(c==L'\\'){++slashes;continue;}
        if(c==L'\"'){out.append(slashes*2+1,L'\\');out+=c;slashes=0;continue;}
        out.append(slashes,L'\\');slashes=0;out+=c;
    }
    out.append(slashes*2,L'\\');out+=L'\"';return out;
}

bool is_admin(){
    BOOL member=FALSE;SID_IDENTIFIER_AUTHORITY nt=SECURITY_NT_AUTHORITY;PSID sid{};
    if(AllocateAndInitializeSid(&nt,2,SECURITY_BUILTIN_DOMAIN_RID,DOMAIN_ALIAS_RID_ADMINS,0,0,0,0,0,0,&sid)){
        CheckTokenMembership(nullptr,sid,&member);FreeSid(sid);
    }
    return member!=FALSE;
}

std::wstring full_path(const std::wstring&p){
    DWORD n=GetFullPathNameW(p.c_str(),0,nullptr,nullptr);if(!n)return p;
    std::vector<wchar_t>b(n+1);if(!GetFullPathNameW(p.c_str(),static_cast<DWORD>(b.size()),b.data(),nullptr))return p;
    std::wstring r=b.data();while(r.size()>3&&(r.back()==L'\\'||r.back()==L'/'))r.pop_back();return r;
}
std::wstring lower(std::wstring s){std::transform(s.begin(),s.end(),s.begin(),::towlower);return s;}
std::wstring env(const wchar_t*name){DWORD n=GetEnvironmentVariableW(name,nullptr,0);if(!n)return L"";std::vector<wchar_t>b(n);GetEnvironmentVariableW(name,b.data(),n);return b.data();}
bool within(const std::wstring&p,const std::wstring&root){
    if(root.empty())return false;
    auto a=lower(full_path(p)),b=lower(full_path(root));
    if(a==b)return true;
    if(b.back()!=L'\\')b+=L'\\';
    return a.rfind(b,0)==0;
}
bool protected_path(const std::wstring&p){
    auto drive=env(L"SystemDrive");if(!drive.empty()&&lower(full_path(p))==lower(full_path(drive+L"\\")))return true;
    for(const auto&root:{env(L"SystemRoot"),env(L"ProgramFiles"),env(L"ProgramFiles(x86)"),env(L"ProgramData")})if(within(p,root))return true;
    return false;
}

std::wstring current_sid(){
    HANDLE token{};if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return L"";
    DWORD n{};GetTokenInformation(token,TokenUser,nullptr,0,&n);std::vector<BYTE>b(n);
    if(!GetTokenInformation(token,TokenUser,b.data(),n,&n)){CloseHandle(token);return L"";}CloseHandle(token);
    LPWSTR text{};if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(b.data())->User.Sid,&text))return L"";
    std::wstring result=text;LocalFree(text);return result;
}

void ensure_dir(const std::wstring&p){CreateDirectoryW(p.c_str(),nullptr);}
void log_line(const std::wstring&s){
    try{
        std::wstring base=env(L"ProgramData");if(base.empty())base=env(L"TEMP");if(base.empty())return;
        base+=L"\\Nebula10";ensure_dir(base);base+=L"\\Logs";ensure_dir(base);
        std::wofstream f(fs::path(base)/L"forceown.log",std::ios::app);if(f){SYSTEMTIME t{};GetLocalTime(&t);f<<t.wYear<<L"-"<<t.wMonth<<L"-"<<t.wDay<<L" "<<t.wHour<<L":"<<t.wMinute<<L":"<<t.wSecond<<L" | "<<s<<L"\n";}
    }catch(...){ }
}

int run_fixed(const std::wstring&exe,const std::wstring&args){
    std::wstring cmd=quote(exe)+L" "+args;std::vector<wchar_t>b(cmd.begin(),cmd.end());b.push_back(0);
    STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
    log_line(L"EXEC "+exe+L" "+args);
    if(!CreateProcessW(exe.c_str(),b.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&si,&pi)){DWORD e=GetLastError();log_line(L"CREATE FAILED "+std::to_wstring(e));return static_cast<int>(e?e:5);}
    WaitForSingleObject(pi.hProcess,INFINITE);DWORD code{};GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
    log_line(L"EXIT "+std::to_wstring(code));return static_cast<int>(code);
}

int elevate(int argc,wchar_t**argv){
    wchar_t self[32768]{};GetModuleFileNameW(nullptr,self,32768);std::wstring args=L"--elevated --yes";
    for(int i=1;i<argc;++i)if(_wcsicmp(argv[i],L"--yes")&&_wcsicmp(argv[i],L"--elevated"))args+=L" "+quote(argv[i]);
    auto r=reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"runas",self,args.c_str(),nullptr,SW_SHOWNORMAL));return r>32?0:5;
}
void pause_shell(bool shell){if(shell){std::wcout<<L"\nPress Enter to close...";std::wstring line;std::getline(std::wcin,line);}}

}

int wmain(int argc,wchar_t**argv){
    bool dry=false,recursive=true,allowSystem=false,yes=false,shell=false,endOptions=false;
    std::vector<std::wstring> targets;
    for(int i=1;i<argc;++i){std::wstring a=argv[i];
        if(!endOptions&&a==L"--"){endOptions=true;continue;}
        if(!endOptions&&a==L"--help"){help();return 0;}
        if(!endOptions&&a==L"--dry-run"){dry=true;continue;}
        if(!endOptions&&a==L"--no-recursive"){recursive=false;continue;}
        if(!endOptions&&a==L"--allow-system"){allowSystem=true;continue;}
        if(!endOptions&&a==L"--yes"){yes=true;continue;}
        if(!endOptions&&a==L"--shell"){shell=true;continue;}
        if(!endOptions&&a==L"--elevated")continue;
        if(!endOptions&&a.rfind(L"--",0)==0){std::wcerr<<L"Unknown option: "<<a<<L"\n";return 2;}
        targets.push_back(full_path(a));
    }
    if(targets.empty())targets.push_back(full_path(fs::current_path().wstring()));
    bool hasProtected=false;
    std::wcout<<L"Nebula ForceOwn\nTargets: "<<targets.size()<<L"\nRecursive: "<<(recursive?L"yes":L"no")<<L"\n";
    for(const auto&t:targets){DWORD attr=GetFileAttributesW(t.c_str());if(attr==INVALID_FILE_ATTRIBUTES){std::wcerr<<L"Not found: "<<t<<L"\n";pause_shell(shell);return 2;}bool p=protected_path(t);hasProtected|=p;std::wcout<<L"  "<<t<<(p?L"  [PROTECTED SYSTEM PATH]":L"")<<L"\n";}
    if(hasProtected&&!allowSystem){std::wcerr<<L"Refused: protected system path selected. Re-run with --allow-system only if you understand the risk.\n";pause_shell(shell);return 3;}
    if(hasProtected)std::wcout<<L"SYSTEM PATH OVERRIDE: ownership changes can damage Windows or installed applications.\n";
    if(dry){std::wcout<<L"DRY-RUN: would run fixed Windows takeown.exe and icacls.exe operations.\nNo ownership or ACL changes made.\n";pause_shell(shell);return 0;}
    if(!nebula::require_nebula_integrity(L"n10forceown.exe")){pause_shell(shell);return 8;}
    if(!yes){std::wcout<<(hasProtected?L"Type FORCEOWN SYSTEM to continue: ":L"Type FORCEOWN to continue: ");std::wstring answer;std::getline(std::wcin,answer);if(answer!=(hasProtected?L"FORCEOWN SYSTEM":L"FORCEOWN")){std::wcout<<L"Cancelled.\n";pause_shell(shell);return 1;}}
    if(!is_admin()){int code=elevate(argc,argv);pause_shell(shell);return code;}
    std::wstring sid=current_sid();if(sid.empty()){std::wcerr<<L"Unable to determine the current user SID.\n";pause_shell(shell);return 5;}
    wchar_t system[MAX_PATH]{};GetSystemDirectoryW(system,MAX_PATH);std::wstring takeown=std::wstring(system)+L"\\takeown.exe",icacls=std::wstring(system)+L"\\icacls.exe";
    int failures=0;
    for(const auto&t:targets){DWORD attr=GetFileAttributesW(t.c_str());bool dir=(attr&FILE_ATTRIBUTE_DIRECTORY)!=0;
        std::wstring own=L"/F "+quote(t);if(dir&&recursive)own+=L" /R /D Y /SKIPSL";
        int a=run_fixed(takeown,own);
        std::wstring grant=quote(t)+L" /grant:r \"*"+sid+(dir?L":(OI)(CI)F\"":L":F\"")+L" /C /L";if(dir&&recursive)grant+=L" /T";
        int b=run_fixed(icacls,grant);if(a||b){++failures;std::wcerr<<L"Failed: "<<t<<L" (takeown="<<a<<L", icacls="<<b<<L")\n";}else std::wcout<<L"Owned and granted Full Control: "<<t<<L"\n";
    }
    std::wcout<<(failures?L"Finished with errors.":L"Finished.")<<L" Log: %ProgramData%\\Nebula10\\Logs\\forceown.log\n";pause_shell(shell);return failures?6:0;
}
