#!/usr/bin/env python
"""Non-networking behavior and policy tests for native N10Store."""
from pathlib import Path
import hashlib
import shutil, subprocess, sys, tempfile

root=Path(sys.argv[1]).resolve()
store=root/'N10Store.exe'
assert store.is_file(),f'missing {store}'
assert hashlib.sha256(store.read_bytes()).hexdigest()=='9ca2fcaeab4388125efa3863e79d3bc5ffa13f4621fa8617e6f9c315e352724f','preserved old N10Store.exe changed'

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
    assert 'DRY-RUN' in out and 'googlechrome' in out and 'choco.exe' in out,out
    assert 'No package changes made' in out,out

out=run('install','Totally.Arbitrary.Package','--dry-run','--yes',code=2)
assert 'not in the curated N10Store catalog' in out,out
assert 'chocolatey' in run('choco-status').lower()
out=run('setup-choco','--dry-run')
assert 'DRY-RUN' in out and 'Tools\\choco' in out,out

setup=(Path(__file__).parents[1]/'src/setup.cpp').read_text(encoding='utf-8')
toolbox=(Path(__file__).parents[1]/'src/n10toolbox.cpp').read_text(encoding='utf-8')
assert 'N10Store.exe' in setup and 'N10Store.exe' in toolbox
assert 'N10Store.ico' in setup
assert 'Nebula Store.lnk' in setup,'Store Desktop/Start Menu shortcuts missing'
assert 'SetIconLocation' in setup and 'N10Store.ico' in setup
print(f'store_tests: PASS (preserved old EXE, {len(catalog_lines)} curated packages, safe Chocolatey dry-runs)')
if isolated is not None:
    isolated.cleanup()
