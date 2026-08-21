#pragma once
#include "common.hpp"
#include <fstream>
#include <map>
#include <sstream>

namespace nebula {
struct ModInfo {
    std::wstring format=L"NEBULA_VERINFO_V1";
    std::wstring modName=L"Nebula";
    std::wstring modVersion=L"1.0";
    std::wstring buildId=L"N10-UNKNOWN";
    std::wstring codename=L"Unknown";
    std::wstring channel=L"Release";
    std::wstring supportedBuilds=L"19044-26100";
    std::wstring author=L"NoxTheDev";
    bool loaded=false;
};
struct WindowsInfo {
    std::wstring product, edition, displayVersion, build;
    DWORD ubr=0;
    int buildNumber=0;
    int generation=10;
};
inline std::wstring utf8_to_wide(const std::string& s){
    if(s.empty())return L"";
    int n=MultiByteToWideChar(CP_UTF8,0,s.data(),static_cast<int>(s.size()),nullptr,0);
    std::wstring out(n,L'\0');MultiByteToWideChar(CP_UTF8,0,s.data(),static_cast<int>(s.size()),out.data(),n);return out;
}
inline std::string trim_ascii(std::string s){
    auto ws=[](unsigned char c){return c==' '||c=='\t'||c=='\r'||c=='\n';};
    while(!s.empty()&&ws(s.front()))s.erase(s.begin());
    while(!s.empty()&&ws(s.back()))s.pop_back();
    return s;
}
inline bool load_verinfo(const std::wstring& path,ModInfo& m,std::wstring* error=nullptr){
    std::ifstream f(path.c_str(),std::ios::binary);if(!f){if(error)*error=L"Unable to open verinfo.bin: "+path;return false;}
    std::map<std::string,std::string> values;std::string line;
    while(std::getline(f,line)){line=trim_ascii(line);if(line.empty()||line[0]=='#'||line[0]==';')continue;auto p=line.find('=');if(p==std::string::npos)continue;values[trim_ascii(line.substr(0,p))]=trim_ascii(line.substr(p+1));}
    if(values["format"]!="NEBULA_VERINFO_V1"){if(error)*error=L"Unsupported or missing verinfo.bin format.";return false;}
    auto get=[&](const char* k,const std::wstring& d){auto it=values.find(k);return it==values.end()?d:utf8_to_wide(it->second);};
    m.format=get("format",m.format);m.modName=get("mod_name",m.modName);m.modVersion=get("mod_version",m.modVersion);
    m.buildId=get("build_id",m.buildId);m.codename=get("codename",m.codename);m.channel=get("channel",m.channel);
    m.supportedBuilds=get("supported_windows_builds",m.supportedBuilds);m.author=get("author",m.author);m.loaded=true;return true;
}
inline WindowsInfo get_windows_info(){
    WindowsInfo w;const wchar_t* p=L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    w.product=reg_string(HKEY_LOCAL_MACHINE,p,L"ProductName");w.displayVersion=reg_string(HKEY_LOCAL_MACHINE,p,L"DisplayVersion");
    w.build=reg_string(HKEY_LOCAL_MACHINE,p,L"CurrentBuildNumber");w.edition=reg_string(HKEY_LOCAL_MACHINE,p,L"EditionID");w.ubr=reg_dword(HKEY_LOCAL_MACHINE,p,L"UBR");
    try{w.buildNumber=std::stoi(w.build);}catch(...){w.buildNumber=0;}w.generation=w.buildNumber>=22000?11:10;return w;
}
inline std::wstring friendly_edition(const WindowsInfo& w){
    std::wstring s=w.product;for(const auto& prefix:{L"Microsoft Windows 11 ",L"Microsoft Windows 10 ",L"Windows 11 ",L"Windows 10 "})if(s.rfind(prefix,0)==0){s=s.substr(wcslen(prefix));break;}
    return s.empty()?w.edition:s;
}
inline bool build_supported(const ModInfo&m,const WindowsInfo&w){
    auto p=m.supportedBuilds.find(L'-');try{if(p==std::wstring::npos)return w.buildNumber==std::stoi(m.supportedBuilds);int lo=std::stoi(m.supportedBuilds.substr(0,p)),hi=std::stoi(m.supportedBuilds.substr(p+1));return w.buildNumber>=lo&&w.buildNumber<=hi;}catch(...){return false;}
}
inline std::wstring nebula_identity(const ModInfo&m,const WindowsInfo&w){
    return m.modName+L" Windows "+std::to_wstring(w.generation)+L" "+m.buildId+L" | "+friendly_edition(w)+L" | Windows Build "+w.build+L"."+std::to_wstring(w.ubr);
}
inline std::wstring arg_value(int argc,wchar_t**argv,const wchar_t* key){for(int i=1;i+1<argc;i++)if(_wcsicmp(argv[i],key)==0)return argv[i+1];return L"";}
}
