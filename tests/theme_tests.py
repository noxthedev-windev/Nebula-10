#!/usr/bin/env python
"""Non-mutating and sandboxed behavior checks for N10 Themes and ForceOwn."""
from pathlib import Path
import os, subprocess, sys, tempfile

BIN = Path(sys.argv[1])

def run(exe, args=(), env=None, expected=0):
    p = subprocess.run([str(BIN / exe), *args], capture_output=True, text=True,
                       timeout=20, env=env)
    out = p.stdout + p.stderr
    assert p.returncode == expected, (exe, args, p.returncode, out)
    return out

help_text = run("n10themes.exe", ["--help"])
for text in ("N10 Themes", "list", "select-wallpaper", "select-icons",
             r"C:\Windows\NebulaData\Themes", r"Documents\Themes"):
    assert text in help_text, (text, help_text)

with tempfile.TemporaryDirectory(prefix="n10-themes-") as td:
    td = Path(td)
    root = td / "official"
    docs = td / "documents" / "Themes"
    wall = root / "Wallpapers" / "Test Wallpaper"
    icons = root / "Icons" / "Test Icons"
    wall.mkdir(parents=True)
    icons.mkdir(parents=True)
    (wall / "sample.png").write_bytes(b"test-wallpaper")
    (icons / "sample.ico").write_bytes(b"test-icon")
    env = os.environ.copy()
    env["NEBULA_THEME_ROOT"] = str(root)
    env["NEBULA_THEME_DOCUMENTS"] = str(docs)
    listing = run("n10themes.exe", ["list"], env)
    assert "Test Wallpaper" in listing and "Test Icons" in listing, listing
    preview = run("n10themes.exe", ["select-icons", "Test Icons", "--dry-run"], env)
    assert "No files copied" in preview and not docs.exists(), preview
    out = run("n10themes.exe", ["select-icons", "Test Icons"], env)
    assert (docs / "Icons" / "Test Icons" / "sample.ico").read_bytes() == b"test-icon", out

force = run("n10forceown.exe", ["--dry-run", "--", str(BIN / "n10ver.exe")])
assert "Nebula ForceOwn" in force and "No ownership or ACL changes made" in force, force

print("theme_tests: PASS (theme sandbox, dry-run, ForceOwn preview)")