#!/usr/bin/env python
"""Execute only non-mutating CLI paths."""
from pathlib import Path
import os, subprocess, sys, json, tempfile
b = Path(sys.argv[1])
cases = [
 ("n10ver.exe", ["--help"], "Usage:"),
 ("n10ver.exe", ["--json"], '"build"'),
 ("n10toolbox.exe", ["--help"], "Commands:"),
 ("n10toolbox.exe", ["info"], "Computer"),
 ("n10toolbox.exe", ["doctor"], "Nebula System Doctor"),
 ("n10toolbox.exe", ["diagnostics", "summary"], "Diagnostics summary"),
 ("n10toolbox.exe", ["diagnostics", "storage"], "Storage diagnostics"),
 ("n10toolbox.exe", ["diagnostics", "battery"], "Battery diagnostics"),
 ("n10toolbox.exe", ["diagnostics", "display"], "Display diagnostics"),
 ("n10toolbox.exe", ["network", "list"], "Network tool catalog"),
 ("n10toolbox.exe", ["tool", "list"], "Windows tool catalog"),
 ("n10toolbox.exe", ["tool", "computer", "--dry-run"], "Computer Management"),
 ("n10toolbox.exe", ["tool", "diskmgmt", "--dry-run"], "Disk Management"),
 ("n10toolbox.exe", ["tool", "registry", "--dry-run"], "Registry Editor"),
 ("n10toolbox.exe", ["tool", "sysinfo", "--dry-run"], "System Information"),
 ("n10toolbox.exe", ["tool", "reliability", "--dry-run"], "Reliability Monitor"),
 ("n10toolbox.exe", ["tool", "environment", "--dry-run"], "Environment Variables"),
 ("n10toolbox.exe", ["tool", "security", "--dry-run"], "Windows Security"),
 ("n10toolbox.exe", ["maintenance", "list"], "Maintenance catalog"),
 ("n10toolbox.exe", ["maintenance", "startup", "--dry-run"], "Startup Apps"),
 ("n10toolbox.exe", ["maintenance", "troubleshoot", "--dry-run"], "Troubleshooters"),
 ("n10toolbox.exe", ["maintenance", "restore", "--dry-run"], "System Restore"),
 ("n10toolbox.exe", ["localtool", "list"], "Mem Reduct"),
 ("n10toolbox.exe", ["localtool", "launch", "memreduct", "--dry-run"], "Tools\\Mem Reduct"),
 ("n10toolbox.exe", ["localtool", "launch", "openshell", "--dry-run"], "Tools\\OpenShell"),
 ("n10toolbox.exe", ["localtool", "launch", "winxshell", "--dry-run"], "Tools\\WinXShell"),
 ("n10toolbox.exe", ["localtool", "launch", "dwmblur", "--dry-run"], "Tools\\dwmblurglass"),
 ("n10toolbox.exe", ["localtool", "launch", "explorerpp", "--dry-run"], "Explorer++.exe"),
 ("n10toolbox.exe", ["localtool", "launch", "shutup10", "--dry-run"], "ShutUp10.exe"),
 ("n10toolbox.exe", ["localtool", "launch", "neofetch", "--dry-run"], "neofetch.exe"),
 ("n10toolbox.exe", ["localtool", "disable", "memreduct", "--dry-run"], "Disabled"),
 ("n10toolbox.exe", ["privacy", "--dry-run"], "DRY-RUN"),
 ("n10toolbox.exe", ["poweruser", "--dry-run"], "DRY-RUN"),
 ("n10toolbox.exe", ["assets", "--dry-run"], "Assets"),
 ("n10toolbox.exe", ["logs", "--dry-run"], "Logs"),
 ("n10toolbox.exe", ["request", "LONG_PATHS_ON", "--dry-run"], "DRY-RUN"),
 ("n10toolbox.exe", ["themes", "roots"], "N10 Themes roots"),
 ("n10toolbox.exe", ["themes", "list"], "N10 Themes official packs"),
 ("n10toolbox.exe", ["themes", "update", "--dry-run"], "fixed verified Nebula-10-Themes catalog"),
 ("n10toolbox.exe", ["themes", "daily", "--dry-run"], "daily theme update task"),
 ("n10toolbox.exe", ["themes", "daily-remove", "--dry-run"], "remove the daily theme update task"),

 ("n10themes.exe", ["--help"], "Usage:"),
 ("n10themes.exe", ["roots"], "Official root:"),
 ("n10themes.exe", ["list"], "N10 Themes official packs"),

 ("N10Store.exe", ["--help"], "Usage:"),
 ("N10Store.exe", ["install", "chrome", "--dry-run", "--yes"], "No package changes made"),
 ("N10Store.exe", ["setup-choco", "--dry-run"], "Tools\\choco"),
 ("N10Store.exe", ["paths"], "NebulaData\\Store\\Downloads"),
 ("N10Store.exe", ["clear-cache", "--dry-run"], "Clear Store Cache"),
 ("NebulaUserAuth.exe", ["--help"], "No passwords"),
 ("NebulaUserAuthService.exe", ["--help"], "Strict actions"),
 ("NebulaSetup.exe", ["--help"], "Usage:"),
 ("NebulaSetup.exe", ["--install", "--dry-run"], "Finished."),
 ("NebulaSetup.exe", ["--install", "--dry-run", "--components=core,store"], "Selected components"),
 ("NebulaSetup.exe", ["--uninstall", "--dry-run"], "No changes made"),
]
for exe,args,needle in cases:
 p=subprocess.run([str(b/exe),*args],capture_output=True,text=True,timeout=15)
 out=p.stdout+p.stderr
 assert p.returncode==0,(exe,args,p.returncode,out)
 assert needle in out,(exe,needle,out)
 print(f"PASS {exe} {' '.join(args)}")
for args in (["--help"], ["doctor"]):
 p=subprocess.run([str(b/'n10toolbox.exe'),*args],capture_output=True,text=True,timeout=15)
 out=p.stdout+p.stderr
 assert p.returncode==0,(args,p.returncode,out)
 assert 'bgrt' not in out.lower(),(args,out)
p=subprocess.run([str(b/'n10toolbox.exe'),'menu','--dry-run'],input='0\n',
                 capture_output=True,text=True,timeout=15)
out=p.stdout+p.stderr
assert p.returncode==0,(p.returncode,out)
for needle in ('Main Menu','N10 Themes','Goodbye from Nebula ToolBox.'):
 assert needle in out,(needle,out)
assert 'bgrt' not in out.lower(),out
print('PASS n10toolbox.exe redirected menu rendering and exit')
with tempfile.TemporaryDirectory(prefix='n10-toolbox-themes-') as td:
 root=Path(td)/'official'
 docs=Path(td)/'selected'
 pack=root/'Wallpapers'/'Pack With Spaces'
 pack.mkdir(parents=True)
 (pack/'sample.png').write_bytes(b'smoke')
 env=os.environ.copy()
 env['NEBULA_THEME_ROOT']=str(root)
 env['NEBULA_THEME_DOCUMENTS']=str(docs)
 p=subprocess.run([str(b/'n10toolbox.exe'),'menu','--dry-run'],
                  input='9\n5\n1\n\n0\n0\n',capture_output=True,text=True,
                  timeout=15,env=env)
 out=p.stdout+p.stderr
 assert p.returncode==0,(p.returncode,out)
 for needle in ('N10 Themes','Select Wallpaper Pack','Pack With Spaces',
                'DRY-RUN: selection validated','Goodbye from Nebula ToolBox.'):
  assert needle in out,(needle,out)
 assert not docs.exists(),docs
print('PASS n10toolbox.exe redirected N10 Themes pack preview')
# strict rejection happens before pipe/service access
p=subprocess.run([str(b/'NebulaUserAuth.exe'),'CMD.EXE'],capture_output=True,text=True)
assert p.returncode==3 and 'not allowlisted' in (p.stdout+p.stderr)
print("PASS NebulaUserAuth rejects arbitrary action")
print(f"smoke_tests: PASS ({len(cases)+3} non-mutating cases)")
