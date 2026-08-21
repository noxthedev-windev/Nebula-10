#!/usr/bin/env python
"""Non-networking behavior and policy tests for native N10Store."""
from pathlib import Path
import subprocess, sys

root=Path(sys.argv[1]).resolve()
store=root/'N10Store.exe'
assert store.is_file(),f'missing {store}'

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
    assert 'DRY-RUN' in out and 'googlechrome' in out and 'choco.exe' in out,out
    assert 'No package changes made' in out,out

out=run('install','Totally.Arbitrary.Package','--dry-run','--yes',code=2)
assert 'not in the curated N10Store catalog' in out,out
assert 'chocolatey' in run('choco-status').lower()
out=run('setup-choco','--dry-run')
assert 'DRY-RUN' in out and 'Tools\\choco' in out,out

source=(Path(__file__).parents[1]/'src/n10store.cpp').read_text(encoding='utf-8')
for contract in ('_getwch','UP/DOWN or W/S','ENTER select','ESC back','Google.Chrome','ChocolateyInstall','choco.exe'):
    assert contract in source,contract
for forbidden in ('cmd.exe','powershell','ShellExecuteW(nullptr,L"open"'):
    assert forbidden.lower() not in source.lower(),forbidden

setup=(Path(__file__).parents[1]/'src/setup.cpp').read_text(encoding='utf-8')
toolbox=(Path(__file__).parents[1]/'src/n10toolbox.cpp').read_text(encoding='utf-8')
assert 'N10Store.exe' in setup and 'N10Store.exe' in toolbox
assert 'N10Store.ico' in setup
assert 'Nebula Store.lnk' in setup,'Store Desktop/Start Menu shortcuts missing'
for contract in ('setup-choco','Tools\\\\choco','require_nebula_integrity'):
    assert contract in source,contract
for contract in ('ensure_store_shortcuts','FOLDERID_Desktop','FOLDERID_Programs','Nebula Store.lnk'):
    assert contract in source,f'missing Store shortcut self-heal contract: {contract}'
assert 'SetIconLocation' in setup and 'N10Store.ico' in setup
print(f'store_tests: PASS ({len(catalog_lines)} curated packages, safe Chocolatey dry-runs, TUI and icon policy)')
