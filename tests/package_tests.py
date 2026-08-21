#!/usr/bin/env python
"""Validate Nebula10 Fix Version 1.1 layout and command surface."""
from pathlib import Path
import os, subprocess, sys

root=Path(sys.argv[1])
required=[
 "NebulaSetup.exe","n10toolbox.exe","n10ver.exe","N10Store.exe",
 "NebulaUserAuth.exe","NebulaUserAuthService.exe","n10forceown.exe","n10themes.exe",
 "NebulaForceOwnShell.dll","verinfo.bin",
 "Assets/Wallpapers/Nebula-Aurora.png",
 "Assets/Branding/NebulaToolBox.ico",
 "Assets/Branding/N10Store.ico","Assets/Branding/N10Store.png",
 "OfficialThemes/Wallpapers/Nebula-Official/Nebula-Aurora.png",
 "OfficialThemes/Wallpapers/Nebula-Official/Nebula-Midnight.png",
 "OfficialThemes/Wallpapers/Nebula-Official/Nebula-Violet.png",
 "OfficialThemes/Icons/Nebula-Official/README.txt",
 "Tools/choco/choco.exe","Tools/Mem Reduct/memreduct.exe","Tools/OpenShell/StartMenu.exe",
 "Tools/WinXShell/WinXShell.exe","Tools/dwmblurglass/DWMBlurGlass.exe",
 "Tools/Explorer++.exe","Tools/ShutUp10.exe","Tools/neofetch.exe",
 "Tools/choco/LICENSE.txt","Tools/choco/config/chocolatey.config",
 "Tools/Mem Reduct/License.txt","Tools/OpenShell/StartMenuDLL.dll",
 "Tools/OpenShell/OpenShellReadme.rtf","Tools/WinXShell/WinXShell.jcfg",
 "Tools/WinXShell/wxsStub.dll","Tools/dwmblurglass/DWMBlurGlassExt.dll",
 "Tools/dwmblurglass/data/config.ini",
 "ThemeUpdater/Update-N10Themes.ps1","ThemeUpdater/Install-DailyUpdater.ps1",
]
missing=[p for p in required if not (root/p).is_file()]
assert not missing,missing
for retired in ("n10hash.exe","n10pathinfo.exe","n10locks.exe"):
 assert not (root/retired).exists(),retired
bgrt_payloads=[p.relative_to(root) for p in root.rglob('*') if 'bgrt' in p.name.lower()]
assert not bgrt_payloads,f"BGRT payloads must be absent from the public package: {bgrt_payloads}"
for generated in (
 "Tools/choco/logs", "Tools/choco/config/chocolatey.config.backup",
 "Tools/OpenShell/currentuser.reg", "Tools/OpenShell/localmachine.reg",
 "Tools/OpenShell/Start Menu Settings.lnk", "Tools/OpenShell/Start Screen.lnk",
):
 assert not (root/generated).exists(),f"mutable or machine-specific tool state leaked into package: {generated}"
choco_env=os.environ.copy()
for key in list(choco_env):
    if key.lower()=="chocolateyinstall": choco_env.pop(key,None)
choco_root=(root/"Tools/choco").resolve()
choco_env["ChocolateyInstall"]=str(choco_root)
p=subprocess.run([str(choco_root/"choco.exe"),"--version"],cwd=choco_root,env=choco_env,capture_output=True,text=True,timeout=30)
assert p.returncode==0,(p.returncode,p.stdout+p.stderr)
assert p.stdout.strip()=="2.6.0",p.stdout+p.stderr
print('package_tests: PASS (layout, no BGRT payloads, bundled Chocolatey)')
