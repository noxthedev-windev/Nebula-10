#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
name="Nebula10-Fix-1.1"
stage_root="$root/dist/.package-stage"
out="$stage_root/$name"
tools="$root/../tools"
tool_dirs=("choco" "Mem Reduct" "OpenShell" "WinXShell" "dwmblurglass")
tool_files=("Explorer++.exe" "ShutUp10.exe" "neofetch.exe")
rm -rf "$stage_root"
rm -f "$root/dist/Nebula10-Native-2.1.0.zip" "$root/dist/Nebula10-Fix-1.0.zip"
mkdir -p "$out/Assets"
for exe in n10ver.exe n10toolbox.exe n10forceown.exe N10Store.exe NebulaUserAuth.exe NebulaUserAuthService.exe NebulaSetup.exe; do cp "$root/build/Release/$exe" "$out/"; done
cp "$root/build/Release/NebulaForceOwnShell.dll" "$out/"
cp "$root"/{verinfo.bin,README.md,SECURITY.md,LICENSE,LICENSES.md,THIRD_PARTY_TOOLS.md} "$out/"
cp -R "$root/assets/." "$out/Assets/"
mkdir -p "$out/Tools"
for item in "${tool_dirs[@]}"; do
  test -d "$tools/$item" || { echo "ERROR: reviewed tool directory missing: $tools/$item" >&2; exit 8; }
  cp -R "$tools/$item" "$out/Tools/"
done
for item in "${tool_files[@]}"; do
  test -f "$tools/$item" || { echo "ERROR: reviewed tool file missing: $tools/$item" >&2; exit 8; }
  cp "$tools/$item" "$out/Tools/"
done
# Never publish mutable source-machine state or stale path-specific shortcuts.
rm -rf "$out/Tools/choco/logs"
rm -f "$out/Tools/choco/config/chocolatey.config.backup" \
  "$out/Tools/OpenShell/currentuser.reg" \
  "$out/Tools/OpenShell/localmachine.reg" \
  "$out/Tools/OpenShell/Start Menu Settings.lnk" \
  "$out/Tools/OpenShell/Start Screen.lnk"
(cd "$stage_root" && cmake -E tar cf "../$name.zip" --format=zip "$name")
printf 'Created %s\n' "$root/dist/$name.zip"
