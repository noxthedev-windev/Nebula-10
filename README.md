# Nebula10 Native 2.1 — Fix Version 1.1

**Fix Version 1.1** adds N10Store, configurable local tools, Store shortcuts, and integrity checks.

Nebula10 Native is a Windows 10/11 terminal customization and identity pack made by **NoxTheDev**. Its public tools are native C++17 console executables.

## What Nebula Setup changes

`NebulaSetup.exe` is the installer. It requests genuine Windows administrator approval once, then:

> **Extract the entire release ZIP first.** `NebulaSetup.exe` is part of a multi-file package and must remain beside the other native executables and `verinfo.bin`. Do not copy only Setup and do not launch it from Windows' ZIP preview. Setup performs this payload preflight before UAC and lists missing files if the package is incomplete.

- Installs the suite under `C:\Program Files\Nebula10`
- Deploys `verinfo.bin` and the original Nebula artwork
- Applies the Nebula version identity and supported OEM branding by default
- Registers a supported Settings/About OEM manufacturer (`NoxTheDev`) and model string without `Windows` in the Nebula product name, such as:

```text
Nebula 10 N10-2608-A2 | IoT Enterprise LTSC 2021 | Build 19044.3324
```

- Stores the custom mod identity under `HKLM\SOFTWARE\Nebula10\Identity`
- Preserves the real licensed Windows edition and build; it does not modify activation or Microsoft system binaries
- Installs Nebula UserAuth Service
- Adds the install directory to machine `PATH`
- Creates only the **Nebula ToolBox** Desktop and Start Menu shortcuts
- Uses the supplied monitor/tile artwork as a multi-size `NebulaToolBox.ico` shortcut icon
- Removes legacy **N10 Version** and **Uninstall Nebula10** shortcuts during install/repair
- Preserves prior OEM Manufacturer/Model values and restores them during uninstall

Preview without changes:

```bat
NebulaSetup.exe --install --dry-run
```

Live install/repair opens a native component selector before elevation. **Core** is required; **Nebula Store** and **Local Tools** are selected by default and can be toggled. Automation can use a strict component list:

```bat
NebulaSetup.exe --install --components=core,store,tools
NebulaSetup.exe --install --components=core,store
NebulaSetup.exe --install --components=core
```

When Setup completes in a real console it prints `Finished.` and waits at `Press Enter to close...`, so the result remains visible. Scripted deployments can disable that pause:

```bat
NebulaSetup.exe --install --no-pause
```

Install or remove:

```bat
NebulaSetup.exe --install
NebulaSetup.exe --repair
NebulaSetup.exe --uninstall
```

### Copy diagnostics and repair

Setup treats payload files and directories already present at the same source/destination path as installed instead of reporting `Copy failed: File exists`. This supports repair launched from `C:\Program Files\Nebula10`.

Repair/update uses native Windows overwrite semantics rather than MinGW's broken `std::filesystem::copy_file(overwrite_existing)` path, which returned error 17 for every existing destination. Setup stops the UserAuth service before updating it. If a running ToolBox keeps an executable locked, Setup stages the replacement for the next reboot and reports that Windows must be restarted.

To test one packaged payload without changing services, the registry, PATH, shortcuts, or boot configuration:

```bat
NebulaSetup.exe --copy-diagnostics "C:\path\to\destination" verinfo.bin
```

On a genuine failure, Setup reports the complete source path, destination path, numeric error code, and Windows error text. Extract the ZIP before installation; do not launch Setup from inside the compressed-folder preview.

## verinfo.bin

`verinfo.bin` is a UTF-8 `key=value` manifest despite its `.bin` extension. You can edit it before packaging:

```ini
format=NEBULA_VERINFO_V1
mod_name=Nebula
mod_version=2.1.0
build_id=N10-2608-A2
codename=Andromeda
channel=Developer Preview
supported_windows_builds=19044-26100
author=NoxTheDev
```

The format marker is required. `n10ver` validates the host build against the declared supported range.

## N10 Version

```bat
n10ver
n10ver --json
n10ver --verinfo C:\path\to\verinfo.bin
```

Example heading:

```text
Nebula Windows 10 N10-2608-A2 | IoT Enterprise LTSC 2021 | Windows Build 19044.3324
```

It also shows the Nebula version, build ID, codename, channel, supported host range, compatibility result, real Windows product, display version, and real edition ID. Windows 10 versus 11 is determined from the genuine host build number.

The Settings/About OEM model and diagnostic heading intentionally differ: Settings displays the branded product name `Nebula 10`, while `n10ver` separately identifies the genuine underlying Windows host and licensed edition.

## Nebula ToolBox TUI

Run this with no arguments:

```bat
n10toolbox
```

It opens a native full-screen settings TUI inspired by the interaction model of [AME Settings CLI](https://github.com/Ameliorated-LLC/ame-settings-cli), with original Nebula code and branding. Use **Up/Down** or **W/S** to move, **Enter** to open a category, and **Escape** to return. The active row is highlighted in green, each setting has secondary context, and every category opens a nested menu.

Main categories:

1. System & Identity — dashboard, N10 version, System Doctor
2. Customize & Tune — privacy, balanced, power-user profiles, wallpapers
3. Diagnostics — system, storage, battery, and display reports
4. Network Center — connections, adapters, Firewall, proxy, Wi-Fi
5. Windows Tools — Settings, Task Manager, Control Panel, Event Viewer, Services, Device Manager, installed apps, DirectX diagnostics, Resource Monitor, Computer Management, Disk Management, Registry Editor, System Information, Reliability Monitor, Performance Monitor, Environment Variables, Optional Features, Windows Security, and Power Options
6. Maintenance — Disk Cleanup, Optimize Drives, Storage, Windows Update, System Protection, Startup Apps, Troubleshooters, Backup Settings, and System Restore
7. Machine Features — narrow allowlisted UserAuth actions
8. Recovery & Files — rollback, logs, assets, command help

The header includes an original NEBULA ASCII logo, Nebula build ID, ToolBox version, current mode, and keyboard controls.

Long categories use a paged viewport with `more above`, `more below`, and `Showing X-Y of N` indicators so selection remains visible in a standard console window.

## N10Store

`N10Store.exe` is a maintained native keyboard-controlled terminal Store backed by the bundled Chocolatey client. It contains a curated catalog of browsers, utilities, development tools, media applications, gaming clients, communication apps, security tools, and system diagnostics. Google Chrome is included as `chrome` / `googlechrome`.

```bat
N10Store
N10Store list
N10Store search browser
N10Store info chrome
N10Store install chrome --dry-run
N10Store install chrome
N10Store upgrade chrome
N10Store uninstall chrome
N10Store choco-status
N10Store setup-choco --dry-run
N10Store setup-choco
N10Store paths
N10Store clear-cache --dry-run
N10Store clear-cache
```

N10Store accepts only Chocolatey package IDs compiled into its curated catalog. It does not accept arbitrary package IDs or commands. Install, upgrade, and uninstall show the exact fixed operation; `--dry-run` performs no network or package changes. Chocolatey cache and installer-temporary data are routed beneath `C:\Windows\NebulaData\Store\Cache` and `C:\Windows\NebulaData\Store\Downloads`. **Clear Store Cache** removes only data beneath those fixed roots. Store creates missing per-user Desktop and Start Menu shortcuts when run from an enrolled Nebula installation.

`n10forceown.exe` remains the guarded native ownership tool. Explorer loads the packaged `NebulaForceOwnShell.dll` command handler so its title reflects the current selection: **ForceOwn this file**, **ForceOwn this folder**, or **ForceOwn all**. The handler passes selected paths only as quoted data arguments; ForceOwn performs its own target preview, protected-path checks, confirmation, logging, and Windows elevation. The retired SHA, Path Info, and Lock Check tools are removed during repair.

## N10 Themes

N10 Themes browses and selects official wallpaper and icon packs. Official content is held in the protected catalog store at `C:\Windows\NebulaData\Themes`; selected packs are copied to `%USERPROFILE%\Documents\Themes\Wallpapers` or `%USERPROFILE%\Documents\Themes\Icons`. Icon packs are staged for compatible Nebula components and are never applied by patching Windows system DLLs.

```bat
n10themes roots
n10themes list
n10themes select-wallpaper Nebula-Official --dry-run
n10themes select-icons Nebula-Official --dry-run
n10toolbox themes update --dry-run
n10toolbox themes daily --dry-run
```

The downloadable catalog lives in the separate public `noxthedev-windev/Nebula-10-Themes` repository. Its updater accepts only fixed GitHub URLs and strict relative paths, verifies every SHA-256 before installation, preserves unknown/local packs, and can run as a daily scheduled task. ToolBox verifies the packaged updater scripts against Setup's enrolled integrity records before launching them.

All menu actions are still available as commands. Use `n10toolbox --help` for the complete list.

Useful automation examples:

```bat
n10toolbox doctor
n10toolbox diagnostics summary
n10toolbox diagnostics storage
n10toolbox diagnostics battery
n10toolbox diagnostics display
n10toolbox network list
n10toolbox tool list
n10toolbox maintenance list
n10toolbox poweruser --dry-run
```

## Nebula UserAuth

Privileged ToolBox operations open `NebulaUserAuth.exe` in a separate real console using `CREATE_NEW_CONSOLE`. It displays the exact allowlisted action and accepts literal `APPROVE` or cancellation. It never collects a password.

The service accepts only:

```text
LONG_PATHS_ON
LONG_PATHS_OFF
OEM_BRANDING_ON
OEM_BRANDING_OFF
```

It is not an arbitrary elevated command runner and does not replace or patch Windows `consent.exe`.

## Build, test, and package

```sh
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 6
ctest --test-dir build --output-on-failure
./scripts/package-release.sh
```

The build compiles the maintained native N10Store source into `build/Release`.

Tests use only read-only and dry-run behavior. They do not install services, write the registry, change PATH, create shortcuts, change wallpaper, or modify boot configuration.
