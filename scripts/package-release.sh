#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
name="Nebula10-Fix-1.1"
stage_root="$root/dist/.package-stage"
out="$stage_root/$name"
bgrt="$root/third_party/NebulaBGRT"
tools="$root/../tools"
rm -rf "$stage_root"
rm -f "$root/dist/Nebula10-Native-2.1.0.zip" "$root/dist/Nebula10-Fix-1.0.zip"
mkdir -p "$out/Assets" "$out/NebulaBGRT/Runtime" "$out/NebulaBGRT/Source" \
  "$out/OfficialThemes/Wallpapers/Nebula-Official" \
  "$out/OfficialThemes/Icons/Nebula-Official"
for exe in n10ver.exe n10toolbox.exe n10forceown.exe n10themes.exe N10Store.exe NebulaBGRT.exe NebulaUserAuth.exe NebulaUserAuthService.exe NebulaSetup.exe; do cp "$root/build/Release/$exe" "$out/"; done
cp "$root/build/Release/NebulaForceOwnShell.dll" "$out/"
cp "$root"/{verinfo.bin,README.md,SECURITY.md,LICENSE,LICENSES.md,THIRD_PARTY_TOOLS.md} "$out/"
cp -R "$root/assets/." "$out/Assets/"
cp "$root/assets/Wallpapers/"* "$out/OfficialThemes/Wallpapers/Nebula-Official/"
cp "$root/assets/Themes/Icons/Nebula Official/README.txt" "$out/OfficialThemes/Icons/Nebula-Official/README.txt"
mkdir -p "$out/Tools"
for item in "choco" "Mem Reduct" "OpenShell" "WinXShell" "dwmblurglass"; do cp -R "$tools/$item" "$out/Tools/"; done
for item in "Explorer++.exe" "ShutUp10.exe" "neofetch.exe"; do cp "$tools/$item" "$out/Tools/"; done
cp -R "$root/theme_catalog/scripts" "$out/ThemeUpdater"
# Public BGRT runtime: command controller calls engine.exe in batch mode only.
cp "$bgrt/publish/NebulaBGRT-Setup.exe" "$out/NebulaBGRT/Runtime/engine.exe"
cp "$bgrt"/{config.txt,splash.bmp,certificate.cer,shim.md,LICENSE,README.md,NEBULA_NOTICE.md} "$out/NebulaBGRT/Runtime/"
cp -R "$bgrt/shim-signed" "$out/NebulaBGRT/Runtime/"
cp -R "$bgrt/efi-signed" "$out/NebulaBGRT/Runtime/"
# GPL source and attribution remain available separately from the runtime.
cp -R "$bgrt/." "$out/NebulaBGRT/Source/"
rm -rf "$out/NebulaBGRT/Source/.git" "$out/NebulaBGRT/Source/bin" "$out/NebulaBGRT/Source/obj" "$out/NebulaBGRT/Source/publish" "$out/NebulaBGRT/Source/dry-run" "$out/NebulaBGRT/Source/test" "$out/NebulaBGRT/Source/release"
rm -f "$out/NebulaBGRT/Source/setup-log.txt" "$out/NebulaBGRT/Source/build-nebulabgrt.log"
# The user-facing package intentionally contains no setup-named BGRT executable.
if python -c "from pathlib import Path; import sys; sys.exit(0 if any(Path(r'$out').rglob('*BGRT*Setup*.exe')) else 1)"; then
  echo "ERROR: setup-named BGRT executable leaked into package" >&2
  exit 9
fi
(cd "$stage_root" && cmake -E tar cf "../$name.zip" --format=zip "$name")
printf 'Created %s\n' "$root/dist/$name.zip"
