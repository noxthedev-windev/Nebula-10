#include "common.hpp"
int wmain(int argc,wchar_t** argv){
 using namespace nebula;
 if(has_arg(argc,argv,L"--help")||argc<2){std::wcout<<L"NebulaUserAuth - approve an allowlisted Nebula10 service request\nUsage: NebulaUserAuth ACTION\nActions: LONG_PATHS_ON, LONG_PATHS_OFF, OEM_BRANDING_ON, OEM_BRANDING_OFF\nNo passwords are requested or stored.\n";return argc<2?2:0;}
 std::wstring action=argv[1]; if(!action_allowed(action)){std::wcerr<<L"Rejected: action is not allowlisted.\n";return 3;}
 if(!require_nebula_integrity(L"NebulaUserAuth.exe"))return 8;
 std::wcout<<L"Nebula10 privileged request\nAction: "<<action<<L"\nThis console never asks for or stores a password.\nType APPROVE to continue, or CANCEL: ";
 std::wstring answer;std::getline(std::wcin,answer);if(answer!=L"APPROVE"){std::wcout<<L"Cancelled.\n";return 1;}
 if(!WaitNamedPipeW(kPipe,5000)){std::wcerr<<L"NebulaUserAuthService is unavailable ("<<GetLastError()<<L").\n";return 4;}
 HANDLE h=CreateFileW(kPipe,GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);if(h==INVALID_HANDLE_VALUE){std::wcerr<<L"Pipe connection failed.\n";return 4;}
 DWORD bytes=0;std::wstring msg=action+L"\n";BOOL ok=WriteFile(h,msg.data(),static_cast<DWORD>(msg.size()*sizeof(wchar_t)),&bytes,nullptr);wchar_t reply[128]{};DWORD got=0;if(ok)ok=ReadFile(h,reply,sizeof(reply)-sizeof(wchar_t),&got,nullptr);CloseHandle(h);
 if(!ok){std::wcerr<<L"Service request failed.\n";return 5;}std::wcout<<reply;return wcsncmp(reply,L"OK",2)==0?0:6;
}
