#!/usr/bin/env python
"""Non-mutating source policy checks for Nebula10."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
required = [
    "CMakeLists.txt", "src/n10ver.cpp", "src/n10toolbox.cpp",
    "src/nebulabgrt.cpp", "src/verinfo.hpp", "verinfo.bin",
    "src/n10store.cpp", "src/n10store.rc",
    "src/user_auth.cpp", "src/auth_service.cpp", "src/setup.cpp",
    "src/n10forceown.cpp", "src/n10themes.cpp", "src/forceown_shell.cpp",
    "README.md", "SECURITY.md", "LICENSES.md",
]
missing = [p for p in required if not (ROOT / p).is_file()]
assert not missing, f"missing required files: {missing}"

service = (ROOT / "src/auth_service.cpp").read_text(encoding="utf-8")
auth = (ROOT / "src/user_auth.cpp").read_text(encoding="utf-8")
toolbox = (ROOT / "src/n10toolbox.cpp").read_text(encoding="utf-8")
setup = (ROOT / "src/setup.cpp").read_text(encoding="utf-8")
store = (ROOT / "src/n10store.cpp").read_text(encoding="utf-8")
assert "LONG_PATHS_ON" in service and "LONG_PATHS_OFF" in service
assert "OEM_BRANDING_ON" in service and "OEM_BRANDING_OFF" in service
assert "CreateNamedPipeW" in service and "ImpersonateNamedPipeClient" in service
assert "APPROVE" in auth and "password" in auth.lower()
assert "CREATE_NEW_CONSOLE" in toolbox
for tui_contract in ("MenuEntry", "_getwch", "UP/DOWN or W/S", "ENTER select", "System & Identity"):
    assert tui_contract in toolbox, f"interactive TUI contract missing: {tui_contract}"
for paging_contract in ("visibleRows", "Showing ", "more above", "more below"):
    assert paging_contract in toolbox, f"long-menu paging contract missing: {paging_contract}"
assert "NebulaBGRT.exe" in toolbox, "native BGRT controller launcher missing"
assert "NebulaBGRT-Setup.exe" not in toolbox, "upstream setup UI must not be public"
for setup_contract in ("--no-bgrt", "NebulaBGRTInstalled", "confirm_bgrt", 'L"-install --yes"', 'L"-uninstall --yes"'):
    assert setup_contract in setup, f"default Setup/BGRT recovery contract missing: {setup_contract}"
for copy_contract in ("CopyFileW", "MOVEFILE_DELAY_UNTIL_REBOOT", "service_stop_for_update", "Restart Windows to finish replacing"):
    assert copy_contract in setup, f"safe repair overwrite contract missing: {copy_contract}"
assert "fs::copy_file(source,destination" not in setup, "MinGW overwrite_existing regression reintroduced"
assert "SetIconLocation" in setup and "NebulaToolBox.ico" in setup, "ToolBox shortcut icon missing"
assert 'destination+L"\\\\n10ver.exe"' not in setup, "N10 Version shortcut must not be created"
assert 'shortcut(folder+L"\\\\Uninstall Nebula10.lnk"' not in setup, "only ToolBox should be created as a shortcut"
assert "remove_legacy_n10_shortcuts" in setup, "legacy N10 Version shortcuts must be removed"
common=(ROOT/"src/common.hpp").read_text(encoding="utf-8")
bgrt=(ROOT/"src/nebulabgrt.cpp").read_text(encoding="utf-8")
for contract in ("BitLockerProtection", "query_system_drive_bitlocker", "ProtectionOn", "Unknown"):
    assert contract in common, f"native BitLocker contract missing: {contract}"
setup_flow=setup[setup.index("bool installBootLogo=false;"):]
assert setup_flow.index("query_system_drive_bitlocker") < setup_flow.index("installBootLogo=confirm_bgrt"), "BitLocker must be checked before boot confirmation/mutation"
assert bgrt.index("query_system_drive_bitlocker") < bgrt.index("run_engine(args)"), "direct BGRT command must gate before engine mutation"
private_bgrt=(ROOT/"third_party/NebulaBGRT/src/Setup.cs").read_text(encoding="utf-8")
flow=private_bgrt[private_bgrt.index("protected void RunPrivilegedActions"):]
assert flow.index("HandleBitLocker();") < flow.index("InitEspPath();"), "private engine must check BitLocker before ESP discovery or mutation"
assert '$"-status {systemDrive}"' in private_bgrt, "BitLocker query must target only the Windows system volume"
assert "manage-bde\", \"-status\", true" not in private_bgrt, "global all-volume BitLocker scan causes false positives"
for contract in ("write_integrity_state", "Integrity", "InstallRoot", "require_nebula_integrity"):
    assert contract in setup+common, f"Nebula integrity contract missing: {contract}"
assert 'require_nebula_integrity(L"NebulaUserAuthService.exe")' in service, "LocalSystem service must verify its registered SHA-256 before accepting requests"
assert "author=NoxTheDev" in (ROOT/"verinfo.bin").read_text(encoding="utf-8"), "Nebula author identity must be NoxTheDev"
for forbidden in ("consent.exe", "takeown", "SeDebugPrivilege"):
    assert forbidden.lower() not in (service + auth + toolbox).lower(), forbidden
cmake=(ROOT/"CMakeLists.txt").read_text(encoding="utf-8")
for retired in ("n10hash", "n10pathinfo", "n10locks"):
    assert retired.lower() not in (cmake+toolbox).lower(), f"retired native tool still built or exposed: {retired}"
for retired_exe in ("n10hash.exe", "n10pathinfo.exe", "n10locks.exe"):
    assert retired_exe in setup, f"repair must remove stale installed tool: {retired_exe}"
forceown=(ROOT/"src/forceown_shell.cpp").read_text(encoding="utf-8")
for label in ("ForceOwn this file", "ForceOwn this folder", "ForceOwn all"):
    assert label in forceown, f"dynamic ForceOwn label missing: {label}"
for contract in ("IExplorerCommand", "IObjectWithSelection", "GetCount", "SIGDN_FILESYSPATH"):
    assert contract in forceown, f"native multi-selection shell handler missing: {contract}"
for contract in ("ExplorerCommandHandler", "NebulaForceOwnShell.dll", "MultiSelectModel", "register_forceown_context_menu"):
    assert contract in setup, f"ForceOwn context-menu registration missing: {contract}"
themes=(ROOT/"src/n10themes.cpp").read_text(encoding="utf-8")
for contract in (r"C:\\Windows\\NebulaData\\Themes", r"Documents\\Themes\\Wallpapers", r"Documents\\Themes\\Icons", "select-wallpaper", "select-icons"):
    assert contract in themes, f"N10 Themes contract missing: {contract}"
assert "N10 Themes" in toolbox and "n10themes.exe" in toolbox, "N10 Themes must be exposed in ToolBox"
for contract in ("NebulaData", "OfficialThemes", "register_forceown_context_menu"):
    assert contract in setup, f"Setup theme/context integration missing: {contract}"
for contract in ("configure_daily_theme_update", "Install-DailyUpdater.ps1", "05:23", "integrity_files"):
    assert contract in setup, f"daily verified theme updater integration missing: {contract}"
for updater_integrity in (r'ThemeUpdater\\Update-N10Themes.ps1', r'ThemeUpdater\\Install-DailyUpdater.ps1'):
    assert updater_integrity in setup and updater_integrity in toolbox, f"updater integrity enrollment missing: {updater_integrity}"
for local_tool in ("Mem Reduct", "OpenShell", "WinXShell", "dwmblurglass", "Explorer++.exe", "ShutUp10.exe", "neofetch.exe"):
    assert local_tool.lower() in toolbox.lower(), f"ToolBox local tool missing: {local_tool}"
assert "ToolPreferences" in toolbox and "Enabled" in toolbox, "ToolBox tool configurability missing"
assert "googlechrome" in store and "ChocolateyInstall" in store and "choco" in store.lower()
print("policy_tests: PASS (required files, strict actions, pipe validation, safe launcher)")
