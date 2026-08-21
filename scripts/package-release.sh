#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
name="Nebula10-Fix-1.1"
stage_root="$root/dist/.package-stage"
out="$stage_root/$name"
tools="$root/../tools"
rm -rf "$stage_root"
rm -f "$root/dist/Nebula10-Native-2.1.0.zip" "$root/dist/Nebula10-Fix-1.0.zip"
mkdir -p "$out/Assets" \
  "$out/OfficialThemes/Wallpapers/Nebula-Official" \
  "$out/OfficialThemes/Icons/Nebula-Official"
for exe in n10ver.exe n10toolbox.exe n10forceown.exe n10themes.exe N10Store.exe NebulaUserAuth.exe NebulaUserAuthService.exe NebulaSetup.exe; do cp "$root/build/Release/$exe" "$out/"; done
cp "$root/build/Release/NebulaForceOwnShell.dll" "$out/"
cp "$root"/{verinfo.bin,README.md,SECURITY.md,LICENSE,LICENSES.md,THIRD_PARTY_TOOLS.md} "$out/"
cp -R "$root/assets/." "$out/Assets/"
cp "$root/assets/Wallpapers/"* "$out/OfficialThemes/Wallpapers/Nebula-Official/"
cp "$root/assets/Themes/Icons/Nebula Official/README.txt" "$out/OfficialThemes/Icons/Nebula-Official/README.txt"
mkdir -p "$out/Tools"
for item in "choco" "Mem Reduct" "OpenShell" "WinXShell" "dwmblurglass"; do cp -R "$tools/$item" "$out/Tools/"; done
for item in "Explorer++.exe" "ShutUp10.exe" "neofetch.exe"; do cp "$tools/$item" "$out/Tools/"; done
cp -R "$root/theme_catalog/scripts" "$out/ThemeUpdater"
(cd "$stage_root" && cmake -E tar cf "../$name.zip" --format=zip "$name")
printf 'Created %s\n' "$root/dist/$name.zip"
