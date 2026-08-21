#pragma once
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <wbemidl.h>
#include <wincrypt.h>
#include <string>
#include <iostream>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cwctype>

namespace nebula {
enum class BitLockerProtection { ProtectionOff, ProtectionOn, Unknown };

inline BitLockerProtection query_system_drive_bitlocker_manage_bde(){
  wchar_t system[MAX_PATH]{};if(!GetSystemDirectoryW(system,MAX_PATH))return BitLockerProtection::Unknown;
  std::wstring exe=std::wstring(system)+L"\\manage-bde.exe";wchar_t drive[16]{};DWORD driveSize=GetEnvironmentVariableW(L"SystemDrive",drive,16);if(!driveSize||driveSize>=16)return BitLockerProtection::Unknown;
  std::wstring command=L"\""+exe+L"\" -status "+std::wstring(drive);std::vector<wchar_t> mutableCommand(command.begin(),command.end());mutableCommand.push_back(0);
  SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};HANDLE readPipe{},writePipe{};if(!CreatePipe(&readPipe,&writePipe,&security,0))return BitLockerProtection::Unknown;SetHandleInformation(readPipe,HANDLE_FLAG_INHERIT,0);
  STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;startup.hStdOutput=writePipe;startup.hStdError=writePipe;startup.hStdInput=GetStdHandle(STD_INPUT_HANDLE);PROCESS_INFORMATION process{};
  BOOL started=CreateProcessW(exe.c_str(),mutableCommand.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process);CloseHandle(writePipe);if(!started){CloseHandle(readPipe);return BitLockerProtection::Unknown;}
  std::string bytes;char buffer[4096];DWORD read{};while(ReadFile(readPipe,buffer,sizeof(buffer),&read,nullptr)&&read)bytes.append(buffer,buffer+read);CloseHandle(readPipe);WaitForSingleObject(process.hProcess,30000);DWORD code{};GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hThread);CloseHandle(process.hProcess);if(code!=0)return BitLockerProtection::Unknown;
  int length=MultiByteToWideChar(CP_OEMCP,0,bytes.data(),static_cast<int>(bytes.size()),nullptr,0);std::wstring output(length,L'\0');if(length)MultiByteToWideChar(CP_OEMCP,0,bytes.data(),static_cast<int>(bytes.size()),output.data(),length);std::transform(output.begin(),output.end(),output.begin(),[](wchar_t c){return static_cast<wchar_t>(std::towlower(c));});
  if(output.find(L"protection on")!=std::wstring::npos||output.find(L"fully encrypted")!=std::wstring::npos||output.find(L"encryption in progress")!=std::wstring::npos||output.find(L"decryption in progress")!=std::wstring::npos||output.find(L"encryption paused")!=std::wstring::npos||output.find(L"decryption paused")!=std::wstring::npos)return BitLockerProtection::ProtectionOn;
  if(output.find(L"protection off")!=std::wstring::npos&&(output.find(L"fully decrypted")!=std::wstring::npos||output.find(L"bitlocker version:    none")!=std::wstring::npos))return BitLockerProtection::ProtectionOff;
  return BitLockerProtection::Unknown;
}

inline BitLockerProtection query_system_drive_bitlocker() {
  HRESULT initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);bool uninitialize=SUCCEEDED(initialized);
  if(FAILED(initialized)&&initialized!=RPC_E_CHANGED_MODE)return query_system_drive_bitlocker_manage_bde();
  IWbemLocator* locator=nullptr;IWbemServices* services=nullptr;IEnumWbemClassObject* results=nullptr;
  BitLockerProtection state=BitLockerProtection::Unknown;
  if(SUCCEEDED(CoCreateInstance(CLSID_WbemLocator,nullptr,CLSCTX_INPROC_SERVER,IID_IWbemLocator,reinterpret_cast<void**>(&locator)))){
    BSTR ns=SysAllocString(L"ROOT\\CIMV2\\Security\\MicrosoftVolumeEncryption");
    if(ns&&SUCCEEDED(locator->ConnectServer(ns,nullptr,nullptr,nullptr,0,nullptr,nullptr,&services))){
      CoSetProxyBlanket(services,RPC_C_AUTHN_WINNT,RPC_C_AUTHZ_NONE,nullptr,RPC_C_AUTHN_LEVEL_CALL,RPC_C_IMP_LEVEL_IMPERSONATE,nullptr,EOAC_NONE);
      wchar_t drive[MAX_PATH]{};GetEnvironmentVariableW(L"SystemDrive",drive,MAX_PATH);std::wstring query=L"SELECT ProtectionStatus, ConversionStatus FROM Win32_EncryptableVolume WHERE DriveLetter='"+std::wstring(drive[0]?drive:L"C:")+L"'";
      BSTR language=SysAllocString(L"WQL"),statement=SysAllocString(query.c_str());
      if(language&&statement&&SUCCEEDED(services->ExecQuery(language,statement,WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY,nullptr,&results))){
        IWbemClassObject* object=nullptr;ULONG returned{};
        if(SUCCEEDED(results->Next(5000,1,&object,&returned))&&returned){VARIANT protection,conversion;VariantInit(&protection);VariantInit(&conversion);LONG p=-1,c=-1;if(SUCCEEDED(object->Get(L"ProtectionStatus",0,&protection,nullptr,nullptr)))p=protection.vt==VT_I4?protection.lVal:protection.vt==VT_UI4?static_cast<LONG>(protection.ulVal):-1;if(SUCCEEDED(object->Get(L"ConversionStatus",0,&conversion,nullptr,nullptr)))c=conversion.vt==VT_I4?conversion.lVal:conversion.vt==VT_UI4?static_cast<LONG>(conversion.ulVal):-1;state=(p==1||c>0)?BitLockerProtection::ProtectionOn:(p==0&&c==0)?BitLockerProtection::ProtectionOff:BitLockerProtection::Unknown;VariantClear(&protection);VariantClear(&conversion);object->Release();}
      }
      if(statement)SysFreeString(statement);
      if(language)SysFreeString(language);
    }
    if(ns)SysFreeString(ns);
  }
  if(results)results->Release();
  if(services)services->Release();
  if(locator)locator->Release();
  if(uninitialize)CoUninitialize();
  return state==BitLockerProtection::Unknown?query_system_drive_bitlocker_manage_bde():state;
 }
inline constexpr wchar_t kPipe[] = L"\\\\.\\pipe\\NebulaUserAuth";
inline constexpr wchar_t kService[] = L"NebulaUserAuthService";
inline bool action_allowed(const std::wstring& a) {
  return a == L"LONG_PATHS_ON" || a == L"LONG_PATHS_OFF" ||
         a == L"OEM_BRANDING_ON" || a == L"OEM_BRANDING_OFF";
}
inline std::wstring reg_string(HKEY root, const wchar_t* path, const wchar_t* name) {
  HKEY k{}; if (RegOpenKeyExW(root,path,0,KEY_READ|KEY_WOW64_64KEY,&k)!=ERROR_SUCCESS) return L"";
  wchar_t b[512]{}; DWORD n=sizeof(b), t=0;
  LONG r=RegQueryValueExW(k,name,nullptr,&t,reinterpret_cast<BYTE*>(b),&n); RegCloseKey(k);
  return r==ERROR_SUCCESS && (t==REG_SZ||t==REG_EXPAND_SZ) ? b : L"";
}
inline DWORD reg_dword(HKEY root, const wchar_t* path, const wchar_t* name, DWORD fallback=0) {
  HKEY k{}; DWORD v=fallback,n=sizeof(v),t=0;
  if(RegOpenKeyExW(root,path,0,KEY_READ|KEY_WOW64_64KEY,&k)==ERROR_SUCCESS){RegQueryValueExW(k,name,nullptr,&t,reinterpret_cast<BYTE*>(&v),&n);RegCloseKey(k);} return v;
}
inline std::wstring escape_json(const std::wstring& s) {
  std::wstring o; for(wchar_t c:s){ if(c==L'"'||c==L'\\') {o+=L'\\';o+=c;} else if(c==L'\n')o+=L"\\n"; else o+=c;} return o;
}
inline std::wstring exe_dir() { wchar_t p[MAX_PATH]{}; GetModuleFileNameW(nullptr,p,MAX_PATH); std::wstring s=p; auto n=s.find_last_of(L"\\/"); return n==s.npos?L".":s.substr(0,n); }
inline std::wstring sha256_file(const std::wstring& path){
  HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr);if(file==INVALID_HANDLE_VALUE)return L"";
  HCRYPTPROV provider{};HCRYPTHASH hash{};std::wstring result;
  if(CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)&&CryptCreateHash(provider,CALG_SHA_256,0,0,&hash)){
    BYTE buffer[65536];DWORD read{};bool ok=true;while(ReadFile(file,buffer,sizeof(buffer),&read,nullptr)&&read){if(!CryptHashData(hash,buffer,read,0)){ok=false;break;}}
    BYTE digest[32];DWORD size=sizeof(digest);if(ok&&CryptGetHashParam(hash,HP_HASHVAL,digest,&size,0)){std::wstringstream text;text<<std::hex<<std::setfill(L'0');for(DWORD i=0;i<size;i++)text<<std::setw(2)<<static_cast<unsigned>(digest[i]);result=text.str();}
  }
  if(hash)CryptDestroyHash(hash);
  if(provider)CryptReleaseContext(provider,0);
  CloseHandle(file);
  return result;
}
inline bool require_nebula_integrity(const wchar_t* executableName){
  const wchar_t* identity=L"SOFTWARE\\Nebula10\\Identity";std::wstring installRoot=reg_string(HKEY_LOCAL_MACHINE,identity,L"InstallRoot"),build=reg_string(HKEY_LOCAL_MACHINE,identity,L"BuildId");
  if(installRoot.empty()||_wcsicmp(build.c_str(),L"N10-2608-A2")!=0||_wcsicmp(installRoot.c_str(),exe_dir().c_str())!=0){std::wcerr<<L"Nebula Systems integrity check failed: this live action requires a verified Nebula10 installation. Run NebulaSetup repair.\n";return false;}
  std::wstring expected=reg_string(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Nebula10\\Integrity",executableName),actual=sha256_file(exe_dir()+L"\\"+executableName);
  if(expected.empty()||actual.empty()||_wcsicmp(expected.c_str(),actual.c_str())!=0){std::wcerr<<L"Nebula Systems anti-tamper check failed for "<<executableName<<L". The file was changed or the integrity record is missing; run NebulaSetup repair.\n";return false;}
  return true;
}
inline bool set_hkcu_dword(const wchar_t* path,const wchar_t* name,DWORD value){HKEY k{};DWORD d{};if(RegCreateKeyExW(HKEY_CURRENT_USER,path,0,nullptr,0,KEY_SET_VALUE,nullptr,&k,&d)!=ERROR_SUCCESS)return false;LONG r=RegSetValueExW(k,name,0,REG_DWORD,reinterpret_cast<const BYTE*>(&value),sizeof(value));RegCloseKey(k);return r==ERROR_SUCCESS;}
inline bool set_hkcu_managed(const wchar_t* id,const wchar_t* path,const wchar_t* name,DWORD type,const BYTE* data,DWORD size){
  HKEY target{};DWORD d{};if(RegCreateKeyExW(HKEY_CURRENT_USER,path,0,nullptr,0,KEY_QUERY_VALUE|KEY_SET_VALUE,nullptr,&target,&d)!=ERROR_SUCCESS)return false;
  HKEY backup{};if(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Nebula10\\Backups",0,nullptr,0,KEY_QUERY_VALUE|KEY_SET_VALUE,nullptr,&backup,&d)!=ERROR_SUCCESS){RegCloseKey(target);return false;}
  std::wstring base=id,marker=base+L"Saved";DWORD oldType{},oldSize{},saved{},n=sizeof(saved);
  if(RegQueryValueExW(backup,marker.c_str(),nullptr,&oldType,reinterpret_cast<BYTE*>(&saved),&n)!=ERROR_SUCCESS){
    LONG q=RegQueryValueExW(target,name,nullptr,&oldType,nullptr,&oldSize);DWORD existed=q==ERROR_SUCCESS?1u:0u;std::vector<BYTE> old(oldSize);
    if(existed&&RegQueryValueExW(target,name,nullptr,&oldType,old.data(),&oldSize)!=ERROR_SUCCESS){RegCloseKey(backup);RegCloseKey(target);return false;}
    RegSetValueExW(backup,(base+L"Existed").c_str(),0,REG_DWORD,reinterpret_cast<BYTE*>(&existed),sizeof(existed));
    RegSetValueExW(backup,(base+L"Type").c_str(),0,REG_DWORD,reinterpret_cast<BYTE*>(&oldType),sizeof(oldType));
    if(existed)RegSetValueExW(backup,(base+L"Data").c_str(),0,REG_BINARY,old.data(),oldSize);
    saved=1;RegSetValueExW(backup,marker.c_str(),0,REG_DWORD,reinterpret_cast<BYTE*>(&saved),sizeof(saved));
  }
  LONG r=RegSetValueExW(target,name,0,type,data,size);RegCloseKey(backup);RegCloseKey(target);return r==ERROR_SUCCESS;
}
inline bool set_hkcu_dword_managed(const wchar_t* id,const wchar_t* path,const wchar_t* name,DWORD value){return set_hkcu_managed(id,path,name,REG_DWORD,reinterpret_cast<const BYTE*>(&value),sizeof(value));}
inline bool set_hkcu_string_managed(const wchar_t* id,const wchar_t* path,const wchar_t* name,const wchar_t* value){return set_hkcu_managed(id,path,name,REG_SZ,reinterpret_cast<const BYTE*>(value),static_cast<DWORD>((wcslen(value)+1)*sizeof(wchar_t)));}
inline bool restore_hkcu_managed(const wchar_t* id,const wchar_t* path,const wchar_t* name){
  HKEY backup{};if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Nebula10\\Backups",0,KEY_QUERY_VALUE|KEY_SET_VALUE,&backup)!=ERROR_SUCCESS)return false;
  std::wstring base=id,marker=base+L"Saved";DWORD existed{},type{},n=sizeof(DWORD),t{};
  if(RegQueryValueExW(backup,marker.c_str(),nullptr,&t,reinterpret_cast<BYTE*>(&existed),&n)!=ERROR_SUCCESS){RegCloseKey(backup);return true;}
  n=sizeof(DWORD);RegQueryValueExW(backup,(base+L"Existed").c_str(),nullptr,&t,reinterpret_cast<BYTE*>(&existed),&n);
  n=sizeof(DWORD);RegQueryValueExW(backup,(base+L"Type").c_str(),nullptr,&t,reinterpret_cast<BYTE*>(&type),&n);
  HKEY target{};DWORD d{};if(RegCreateKeyExW(HKEY_CURRENT_USER,path,0,nullptr,0,KEY_SET_VALUE,nullptr,&target,&d)!=ERROR_SUCCESS){RegCloseKey(backup);return false;}
  LONG r=ERROR_SUCCESS;if(existed){DWORD size{};if(RegQueryValueExW(backup,(base+L"Data").c_str(),nullptr,&t,nullptr,&size)!=ERROR_SUCCESS)r=ERROR_FILE_NOT_FOUND;else{std::vector<BYTE> data(size);RegQueryValueExW(backup,(base+L"Data").c_str(),nullptr,&t,data.data(),&size);r=RegSetValueExW(target,name,0,type,data.data(),size);}}else{r=RegDeleteValueW(target,name);if(r==ERROR_FILE_NOT_FOUND)r=ERROR_SUCCESS;}
  RegCloseKey(target);if(r==ERROR_SUCCESS){RegDeleteValueW(backup,marker.c_str());RegDeleteValueW(backup,(base+L"Existed").c_str());RegDeleteValueW(backup,(base+L"Type").c_str());RegDeleteValueW(backup,(base+L"Data").c_str());}RegCloseKey(backup);return r==ERROR_SUCCESS;
}
inline bool has_arg(int argc,wchar_t** argv,const wchar_t* a){for(int i=1;i<argc;i++)if(_wcsicmp(argv[i],a)==0)return true;return false;}
}
