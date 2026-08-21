#!/usr/bin/env python
"""Non-mutating source policy checks for Nebula10."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
required = [
    "CMakeLists.txt", "src/n10ver.cpp", "src/n10toolbox.cpp",
    "src/verinfo.hpp", "verinfo.bin", "src/n10store.cpp", "src/n10store.rc",
    "src/user_auth.cpp", "src/auth_service.cpp", "src/setup.cpp",
    "src/n10forceown.cpp", "src/forceown_shell.cpp",
    "README.md", "SECURITY.md", "LICENSES.md",
]
missing = [p for p in required if not (ROOT / p).is_file()]
assert not missing, f"missing required files: {missing}"

service = (ROOT / "src/auth_service.cpp").read_text(encoding="utf-8")
auth = (ROOT / "src/user_auth.cpp").read_text(encoding="utf-8")
toolbox = (ROOT / "src/n10toolbox.cpp").read_text(encoding="utf-8")
setup = (ROOT / "src/setup.cpp").read_text(encoding="utf-8")

assert "LONG_PATHS_ON" in service and "LONG_PATHS_OFF" in service
assert "OEM_BRANDING_ON" in service and "OEM_BRANDING_OFF" in service
assert "CreateNamedPipeW" in service and "ImpersonateNamedPipeClient" in service
assert "APPROVE" in auth and "password" in auth.lower()
assert "CREATE_NEW_CONSOLE" in toolbox
for tui_contract in ("MenuEntry", "_getwch", "UP/DOWN or W/S", "ENTER select", "System & Identity"):
    assert tui_contract in toolbox, f"interactive TUI contract missing: {tui_contract}"
for paging_contract in ("visibleRows", "Showing ", "more above", "more below"):
    assert paging_contract in toolbox, f"long-menu paging contract missing: {paging_contract}"
assert "bgrt" not in toolbox.lower(), "ToolBox must not expose BGRT help, doctor, dispatch, or menu UI"
for retired_setup_surface in ("--no-bgrt", "confirm_bgrt", "installBootLogo", 'L"-install --yes"'):
    assert retired_setup_surface not in setup, f"Setup still exposes active BGRT behavior: {retired_setup_surface}"
for copy_contract in ("CopyFileW", "MOVEFILE_DELAY_UNTIL_REBOOT", "service_stop_for_update", "Restart Windows to finish replacing"):
    assert copy_contract in setup, f"safe repair overwrite contract missing: {copy_contract}"
assert "fs::copy_file(source,destination" not in setup, "MinGW overwrite_existing regression reintroduced"
assert "SetIconLocation" in setup and "NebulaToolBox.ico" in setup, "ToolBox shortcut icon missing"
assert 'destination+L"\\\\n10ver.exe"' not in setup, "N10 Version shortcut must not be created"
assert 'shortcut(folder+L"\\\\Uninstall Nebula10.lnk"' not in setup, "only ToolBox should be created as a shortcut"
assert "remove_legacy_n10_shortcuts" in setup, "legacy N10 Version shortcuts must be removed"
common=(ROOT/"src/common.hpp").read_text(encoding="utf-8")
for contract in ("write_integrity_state", "Integrity", "InstallRoot", "require_nebula_integrity"):
    assert contract in setup+common, f"Nebula integrity contract missing: {contract}"
assert 'require_nebula_integrity(L"NebulaUserAuthService.exe")' in service, "LocalSystem service must verify its registered SHA-256 before accepting requests"
assert "author=NoxTheDev" in (ROOT/"verinfo.bin").read_text(encoding="utf-8"), "Nebula author identity must be NoxTheDev"
for forbidden in ("consent.exe", "takeown", "SeDebugPrivilege"):
    assert forbidden.lower() not in (service + auth + toolbox).lower(), forbidden
cmake=(ROOT/"CMakeLists.txt").read_text(encoding="utf-8")
package_script=(ROOT/"scripts/package-release.sh").read_text(encoding="utf-8")
assert "bgrt" not in cmake.lower(), "CMake must not build or install BGRT"
assert "bgrt" not in package_script.lower(), "release packaging must not stage a BGRT payload"
assert "add_executable(N10Store" in cmake, "maintained native Store source must be built"
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
assert "N10 Themes" not in toolbox, "N10 Themes must be removed from ToolBox"
assert "n10themes.exe" not in setup, "N10 Themes payload must not be installed"
assert "ThemeUpdater" not in setup, "Theme updater must not be wired into Setup"
for local_tool in ("Mem Reduct", "OpenShell", "WinXShell", "dwmblurglass", "Explorer++.exe", "ShutUp10.exe", "neofetch.exe"):
    assert local_tool.lower() in toolbox.lower(), f"ToolBox local tool missing: {local_tool}"
assert "ToolPreferences" in toolbox and "Enabled" in toolbox, "ToolBox tool configurability missing"
for setup_tools_contract in ("installed_tool_files", "verify_installed_tools", "remove_retired_tool_payloads"):
    assert setup_tools_contract in setup, f"Setup managed Tools-folder contract missing: {setup_tools_contract}"
for packaging_contract in ("tool_dirs", "tool_files", "chocolatey.config.backup", "currentuser.reg"):
    assert packaging_contract in package_script, f"clean fixed tool packaging contract missing: {packaging_contract}"
store=(ROOT/"src/n10store.cpp").read_text(encoding="utf-8")
for store_contract in (r"C:\\Windows\\NebulaData\\Store", "NEBULA_STORE_DATA_ROOT", "clear-cache", "Clear Store Cache", "--cache-location"):
    assert store_contract in store, f"Store cache/download contract missing: {store_contract}"
for setup_selection_contract in ("InstallSelection", "--components=", "select_install_components", "Selected components"):
    assert setup_selection_contract in setup, f"Setup component selection contract missing: {setup_selection_contract}"
print("policy_tests: PASS (required files, no public BGRT, strict actions, safe launcher)")
