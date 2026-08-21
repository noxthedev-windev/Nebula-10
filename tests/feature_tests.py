#!/usr/bin/env python
"""Behavior tests for the Nebula10 v2 terminal experience (non-mutating)."""
from pathlib import Path
import shutil, subprocess, sys, tempfile

BIN = Path(sys.argv[1])
ROOT = Path(__file__).resolve().parents[1]


def run(exe, args=(), input_text=None, expected=0):
    p = subprocess.run(
        [str(BIN / exe), *args], input=input_text, capture_output=True,
        text=True, timeout=20
    )
    out = p.stdout + p.stderr
    assert p.returncode == expected, (exe, args, p.returncode, out)
    return out

# n10ver must combine mod metadata with genuine host Windows identity.
with tempfile.TemporaryDirectory(prefix="nebula-verinfo-test-") as td:
    manifest = Path(td) / "verinfo.bin"
    manifest.write_text(
        "format=NEBULA_VERINFO_V1\n"
        "mod_name=Nebula\n"
        "mod_version=2.0\n"
        "build_id=N10-TEST-42\n"
        "codename=Andromeda\n"
        "channel=Test\n"
        "supported_windows_builds=19044-26100\n"
        "author=Nox Nebula\n",
        encoding="utf-8",
    )
    out = run("n10ver.exe", ["--verinfo", str(manifest)])
    for needle in ("Nebula Windows", "N10-TEST-42", "Andromeda", "Supported Windows builds"):
        assert needle in out, (needle, out)
    out = run("n10ver.exe", ["--verinfo", str(manifest), "--json"])
    for needle in ('"modName":"Nebula"', '"buildId":"N10-TEST-42"', '"windows"'):
        assert needle in out, (needle, out)

# No-argument ToolBox must be a real interactive TUI with ASCII branding and grouped navigation.
out = run("n10toolbox.exe", input_text="0\n")
for needle in (
    "_   ____________", "NEBULA TOOLBOX", "Nebula10 |", "ToolBox v2.1",
    "> System & Identity", "Dashboard, N10 version, and health",
    "Customize & Tune", "Diagnostics", "UP/DOWN", "W/S", "ENTER select",
    "Goodbye from Nebula ToolBox",
):
    assert needle in out, (needle, out)

# System Doctor must be a read-only readiness report without retired BGRT UI.
out = run("n10toolbox.exe", ["doctor"])
for needle in ("Nebula System Doctor", "verinfo.bin", "UserAuth", "N10Store", "Overall"):
    assert needle in out, (needle, out)
assert "bgrt" not in out.lower(), out

# Setup preview must include mod identity, ForceOwn shell behavior, and the
# protected official theme store without mutating the machine.
out = run("NebulaSetup.exe", ["--install", "--dry-run"])
for needle in (
    "Nebula Windows", "verinfo.bin", "OEM branding", "Finished.",
    "Shortcuts: Nebula ToolBox and Nebula Store",
    "NebulaToolBox.ico", "ForceOwn this file", "ForceOwn this folder", "ForceOwn all",
    r"C:\Windows\NebulaData\Themes", "protected official theme store",
):
    assert needle in out, (needle, out)
settings_model = next((line for line in out.splitlines() if line.startswith("Settings OEM model")), "")
assert settings_model.startswith("Settings OEM model: Nebula 10"), settings_model
assert "Windows" not in settings_model, settings_model
out = run("NebulaSetup.exe", ["--install", "--dry-run", "--components=core,store"])
for needle in ("Selected components", "Core", "Nebula Store", "Local Tools: not selected"):
    assert needle in out, (needle, out)

# Build and Setup source contracts cover the machine-wide 64-bit Explorer
# registration and ensure the DLL never receives the console -municode option.
cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
setup_source = (ROOT / "src/setup.cpp").read_text(encoding="utf-8")
for needle in ("n10forceown", "n10themes", "NebulaForceOwnShell", "SHARED"):
    assert needle in cmake, (needle, cmake)
assert "bgrt" not in cmake.lower(), "CMake must not build or install a BGRT target"
assert "add_link_options(-municode" not in cmake, cmake
for needle in (
    "{7E950195-94A7-4E5D-9C94-E51E8D0F94CD}",
    r"SOFTWARE\\Classes\\AllFilesystemObjects\\shell\\NebulaForceOwn",
    "ExplorerCommandHandler", "MultiSelectModel", "Player", "InprocServer32",
    "ThreadingModel", "Apartment", r"Shell Extensions\\Approved", "KEY_WOW64_64KEY",
    "NebulaForceOwnShell.dll", "n10forceown.exe", "n10themes.exe", "OfficialThemes",
):
    assert needle in setup_source, (needle, setup_source)
assert 'extension!=L".exe"&&extension!=L".dll"' in setup_source, setup_source
remove_for_repair = setup_source.index("remove_forceown_shell_registration();\n    fs::create_directories")
register_after_copy = setup_source.index("register_forceown_context_menu(destination)")
assert remove_for_repair < register_after_copy, (remove_for_repair, register_after_copy)
assert 'schedule_tree_remove(fs::path(L"C:\\\\Windows\\\\NebulaData\\\\Themes"))' not in setup_source

# Repair may be launched from the installed directory. Copying a payload onto
# itself must be treated as already installed rather than ERROR_FILE_EXISTS.
out = run("NebulaSetup.exe", ["--copy-diagnostics", str(BIN), "NebulaSetup.exe"])
assert "Payload copy diagnostics: PASS" in out, out
assert "already in place" in out.lower() and "Copy failed" not in out, out

# Repair/update must overwrite an existing destination. TDM-GCC's
# std::filesystem::copy_file(overwrite_existing) previously returned error 17.
with tempfile.TemporaryDirectory(prefix="nebula-overwrite-") as td:
    old = Path(td) / "verinfo.bin"
    old.write_text("old payload", encoding="utf-8")
    out = run("NebulaSetup.exe", ["--copy-diagnostics", td, "verinfo.bin"])
    assert "Payload copy diagnostics: PASS" in out, out
    assert old.read_bytes() == (BIN / "verinfo.bin").read_bytes()

# Copying only Setup is a common user mistake. It must fail before UAC/install
# with one actionable package-preflight message, not many copy failures.
with tempfile.TemporaryDirectory(prefix="nebula-setup-only-") as td:
    standalone = Path(td) / "NebulaSetup.exe"
    shutil.copy2(BIN / "NebulaSetup.exe", standalone)
    p = subprocess.run(
        [str(standalone), "--install", "--dry-run"], cwd=td,
        capture_output=True, text=True, timeout=20,
    )
    setup_only = p.stdout + p.stderr
    assert p.returncode != 0, setup_only
    for needle in ("INCOMPLETE NEBULA10 PACKAGE", "Extract the entire ZIP", "verinfo.bin", "n10ver.exe"):
        assert needle in setup_only, (needle, setup_only)
    assert "Copy failed" not in setup_only, setup_only

print("feature_tests: PASS (verinfo, interactive TUI, setup identity, no BGRT surface)")
