#!/usr/bin/env python
"""Non-networking behavior and policy tests for native N10Store."""
from pathlib import Path
import shutil, subprocess, sys, tempfile

root=Path(sys.argv[1]).resolve()
store=root/'N10Store.exe'
assert store.is_file(),f'missing {store}'


# Chocolatey writes logs even for read-only status/version probes. When a
# complete packaged Tools tree is supplied, exercise an isolated copy so tests
# never mutate the staged or extracted release being validated.
isolated=None
if (root/'Tools/choco/choco.exe').is_file():
    isolated=tempfile.TemporaryDirectory(prefix='nebula-store-test-')
    isolated_root=Path(isolated.name)
    shutil.copy2(store,isolated_root/'N10Store.exe')
    shutil.copytree(root/'Tools/choco',isolated_root/'Tools/choco')
    store=isolated_root/'N10Store.exe'

def run(*args,code=0,input_text=None):
    p=subprocess.run([str(store),*args],input=input_text,capture_output=True,text=True,timeout=30)
    out=p.stdout+p.stderr
    assert p.returncode==code,(args,p.returncode,out)
    return out

assert 'Usage:' in run('--help')
out=run('list')
assert 'Google Chrome' in out and 'googlechrome' in out,out
for category in ('Browsers','Utilities','Development','Media','Gaming','Communication','Security','System Tools'):
    assert category in out,category
catalog_lines=[line for line in out.splitlines() if ' | ' in line and not line.startswith('SLUG |')]
assert len(catalog_lines)>=40,len(catalog_lines)
assert 'googlechrome' in run('info','chrome')
assert 'Firefox' in run('search','browser')

for action in ('install','upgrade','uninstall'):
    out=run(action,'chrome','--dry-run','--yes')
    assert 'DRY-RUN' in out and 'winget' in out.lower() and 'Google.Chrome' in out,out
    assert '--accept-package-agreements' in out and '--accept-source-agreements' in out,out
    assert 'No package changes made' in out,out

out=run('install','chrome','--dry-run','--yes','--provider=winget')
assert 'winget' in out.lower() and 'Google.Chrome' in out,out
# Winget-only Store: unknown providers are rejected with usage guidance.
out=run('install','chrome','--dry-run','--yes','--provider=bogus',code=2)
assert 'Unknown provider' in out,out

out=run('install','Totally.Arbitrary.Package','--dry-run','--yes',code=2)
assert 'not in the curated N10Store catalog' in out,out
assert 'chocolatey' in run('choco-status').lower()
out=run('setup-choco','--dry-run')
assert 'DRY-RUN' in out and 'Tools\\choco' in out,out
out=run('paths')
assert 'NebulaData\\Store\\Downloads' in out and 'NebulaData\\Store\\Cache' in out,out
out=run('clear-cache','--dry-run')
assert 'Clear Store Cache' in out and 'No files removed' in out,out
out=run('install','chrome','--dry-run','--yes')
assert 'NebulaData\\Store\\Downloads' in out and 'NebulaData\\Store\\Cache' in out,out

p=subprocess.run([str(store),'menu','--dry-run'],input='1\n1\n1\n\n0\n0\n',capture_output=True,text=True,timeout=30)
menu_out=p.stdout+p.stderr
assert p.returncode==0,(p.returncode,menu_out)
assert 'DRY-RUN' in menu_out and 'Google.Chrome' in menu_out,menu_out

setup=(Path(__file__).parents[1]/'src/setup.cpp').read_text(encoding='utf-8')
toolbox=(Path(__file__).parents[1]/'src/n10toolbox.cpp').read_text(encoding='utf-8')
store_src=(Path(__file__).parents[1]/'src/n10store.cpp').read_text(encoding='utf-8')
# Setup (elevated) must pre-create the fixed Store data roots so the
# unelevated Store app never has to mkdir under C:\Windows itself.
assert r'NebulaData\\Store\\Cache' in setup and r'NebulaData\\Store\\Downloads' in setup, \
    'Setup must create the NebulaData Store Cache/Downloads folders'
# Winget availability must be checked with a clear error, and actions must
# report success/failure explicitly instead of a bare exit code.
assert 'winget_on_path' in store_src,'Winget availability check missing'
assert '[ERROR] No winget detected on this PC.' in store_src,'Winget-missing guidance missing'
assert 'aka.ms/getwinget' in store_src,'winget install link missing'
assert 'falling back to bundled Chocolatey' not in store_src,'Store must be winget-only (no silent fallback)'
assert 'installed successfully' in store_src.lower(),'explicit install success feedback missing'
assert 'NebulaSetup repair' in store_src,'Store data-folder recovery guidance missing'
# Live actions must relaunch the Store itself elevated (one UAC prompt) and
# then spawn the provider with CreateProcessW so environment overrides
# (TEMP/TMP/ChocolateyCacheLocation) survive; ShellExecuteExW runas would
# discard them.
assert 'relaunch_elevated' in store_src,'self-elevation relaunch missing'
assert 'CreateProcessW(exe.c_str()' in store_src,'provider must be spawned with inheriting environment'
assert 'ChocolateyCacheLocation' in store_src,'choco cache env override missing'
# The elevated worker runs in a second console whose output would vanish;
# provider output must be captured to a fixed log and echoed into the
# caller's console, and a cancelled UAC prompt must read as cancelled.
assert 'StoreAction.log' in store_src,'provider output must be captured to a fixed log'
assert 'print_action_log' in store_src,'caller console must echo captured provider output'
assert 'UAC prompt was cancelled' in store_src,'UAC cancellation must be reported distinctly'
assert 'N10Store.exe' in setup and 'N10Store.exe' in toolbox
assert 'N10Store.ico' in setup
assert 'Nebula Store.lnk' in setup,'Store Desktop/Start Menu shortcuts missing'
assert 'SetIconLocation' in setup and 'N10Store.ico' in setup
assert 'quality-of-life tools' in setup.lower(), 'Setup must ask about quality-of-life tools from Tools folder'
print(f'store_tests: PASS ({len(catalog_lines)} curated packages, Winget install default, fixed selection, NebulaData cache)')
if isolated is not None:
    isolated.cleanup()
