#include "verinfo.hpp"
int wmain(int argc,wchar_t** argv){
 using namespace nebula;
 if(has_arg(argc,argv,L"--help")||has_arg(argc,argv,L"-h")){
  std::wcout<<L"n10ver - Nebula mod and genuine Windows version information\nUsage: n10ver [--json] [--verinfo PATH] [--help]\n";return 0;
 }
 WindowsInfo w=get_windows_info();if(w.product.empty()||w.build.empty()){std::wcerr<<L"Unable to read Windows version registry data.\n";return 2;}
 std::wstring manifest=arg_value(argc,argv,L"--verinfo");if(manifest.empty())manifest=exe_dir()+L"\\verinfo.bin";
 ModInfo m;std::wstring error;if(!load_verinfo(manifest,m,&error)){std::wcerr<<error<<L"\n";return 3;}
 bool supported=build_supported(m,w);
 if(has_arg(argc,argv,L"--json")){
  std::wcout<<L"{\"modName\":\""<<escape_json(m.modName)<<L"\",\"modVersion\":\""<<escape_json(m.modVersion)
   <<L"\",\"buildId\":\""<<escape_json(m.buildId)<<L"\",\"codename\":\""<<escape_json(m.codename)
   <<L"\",\"channel\":\""<<escape_json(m.channel)<<L"\",\"supportedWindowsBuilds\":\""<<escape_json(m.supportedBuilds)
   <<L"\",\"hostSupported\":"<<(supported?L"true":L"false")<<L",\"windows\":{\"generation\":"<<w.generation
   <<L",\"product\":\""<<escape_json(w.product)<<L"\",\"edition\":\""<<escape_json(w.edition)
   <<L"\",\"displayVersion\":\""<<escape_json(w.displayVersion)<<L"\",\"build\":\""<<escape_json(w.build)
   <<L"\",\"ubr\":"<<w.ubr<<L"}}\n";
 }else{
  std::wcout<<nebula_identity(m,w)<<L"\n\n"
   <<L"Nebula version          : "<<m.modVersion<<L"\n"
   <<L"Nebula build ID         : "<<m.buildId<<L"\n"
   <<L"Codename                : "<<m.codename<<L"\n"
   <<L"Channel                 : "<<m.channel<<L"\n"
   <<L"Supported Windows builds: "<<m.supportedBuilds<<L"\n"
   <<L"Host compatibility      : "<<(supported?L"Supported":L"Outside declared range")<<L"\n"
   <<L"Real Windows product    : "<<w.product<<L" "<<w.displayVersion<<L"\n"
   <<L"Real edition ID         : "<<w.edition<<L"\n"
   <<L"Author                  : "<<m.author<<L"\n";
 }
 return 0;
}
