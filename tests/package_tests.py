#!/usr/bin/env python
"""Validate Nebula10 Fix Version 1.1 layout and command surface."""
from pathlib import Path
import os, subprocess, sys, tempfile

root=Path(sys.argv[1])
required=[
 "NebulaSetup.exe","n10toolbox.exe","n10ver.exe","N10Store.exe","NebulaBGRT.exe",
 "NebulaUserAuth.exe","NebulaUserAuthService.exe","n10forceown.exe","n10themes.exe",
 "NebulaForceOwnShell.dll","verinfo.bin",
 "NebulaBGRT/Runtime/engine.exe","NebulaBGRT/Runtime/config.txt",
 "NebulaBGRT/Runtime/splash.bmp","NebulaBGRT/Runtime/efi-signed/bootx64.efi",
 "NebulaBGRT/Source/src/Setup.cs","NebulaBGRT/Source/LICENSE",
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
 "ThemeUpdater/Update-N10Themes.ps1","ThemeUpdater/Install-DailyUpdater.ps1",
]
missing=[p for p in required if not (root/p).is_file()]
assert not missing,missing
for retired in ("n10hash.exe","n10pathinfo.exe","n10locks.exe"):
 assert not (root/retired).exists(),retired
leaks=[p for p in root.rglob('*.exe') if 'bgrt' in p.name.lower() and 'setup' in p.name.lower()]
assert not leaks,leaks
choco_env=os.environ.copy()
for key in list(choco_env):
    if key.lower()=="chocolateyinstall": choco_env.pop(key,None)
choco_root=(root/"Tools/choco").resolve()
choco_env["ChocolateyInstall"]=str(choco_root)
p=subprocess.run([str(choco_root/"choco.exe"),"--version"],cwd=choco_root,env=choco_env,capture_output=True,text=True,timeout=30)
assert p.returncode==0,(p.returncode,p.stdout+p.stderr)
assert p.stdout.strip()=="2.6.0",p.stdout+p.stderr
with tempfile.TemporaryDirectory(prefix="nebula-bgrt-dryrun-") as td:
    env=os.environ.copy()
    env["NEBULA10_LOG_DIR"]=str(Path(td)/"logs")
    p=subprocess.run([str(root/'NebulaBGRT.exe'),'-install','--dry-run'],env=env,capture_output=True,text=True,timeout=60)
    out=p.stdout+p.stderr
    assert p.returncode==0,(p.returncode,out)
    assert "Completed action 'install' successfully" in out,out
    assert "This was a dry run" in out,out

# The private engine starts non-elevated and may live under Program Files.
# Logging must never target its runtime directory or crash before elevation.
with tempfile.TemporaryDirectory(prefix="nebula-bgrt-log-") as td:
    temp=Path(td)
    blocked=temp/"not-a-directory"
    blocked.write_text("force Directory.CreateDirectory failure",encoding="utf-8")
    fallback=temp/"fallback"
    fallback.mkdir()
    env=os.environ.copy()
    env["NEBULA10_LOG_DIR"]=str(blocked)
    env["TEMP"]=str(fallback)
    env["TMP"]=str(fallback)
    engine=root/"NebulaBGRT/Runtime/engine.exe"
    p=subprocess.run([str(engine),"log-test"],cwd=engine.parent,env=env,capture_output=True,text=True,timeout=30)
    out=p.stdout+p.stderr
    assert p.returncode==0,(p.returncode,out)
    assert "NebulaBGRT logging self-test passed" in out,out
    fallback_log=fallback/"Nebula10/NebulaBGRT/setup-log.txt"
    assert fallback_log.is_file(),fallback_log
    assert "logging self-test" in fallback_log.read_text(encoding="utf-8")
    assert not (engine.parent/"setup-log.txt").exists(),"runtime directory log regression"
print('package_tests: PASS (layout, no public BGRT setup EXE, command-driven BGRT dry-run)')
