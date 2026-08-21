# Nebula10 Local Tools Payload

This package includes selected files supplied from the project owner's folder:

`C:\Users\Nox Nebula\Desktop\nebula10\tools`

Nebula10 does not claim ownership of these third-party components. Their original names, notices, signatures, and supporting files are preserved under `Tools`.

## Included allowlisted ToolBox entries

- Mem Reduct — `Tools\Mem Reduct\memreduct.exe`; bundled notice: `Tools\Mem Reduct\License.txt`
- Open-Shell — `Tools\OpenShell\StartMenu.exe`; bundled documentation: `Tools\OpenShell\OpenShellReadme.rtf` and `OpenShell.chm`
- WinXShell — `Tools\WinXShell\WinXShell.exe`; configuration and scripts are retained in the same folder
- DWMBlurGlass — `Tools\dwmblurglass\DWMBlurGlass.exe`; original folder contents are retained
- Explorer++ — `Tools\Explorer++.exe`
- O&O ShutUp10 — `Tools\ShutUp10.exe`
- NeoFetch — `Tools\neofetch.exe`

## Chocolatey

Nebula Store uses the supplied Chocolatey 2.6.0 runtime under `Tools\choco`. Chocolatey's license and supporting notices remain in that directory, including `LICENSE.txt` and the license files in `Tools\choco\tools`.

Nebula Store supports only compiled-in package IDs. Package installation, upgrades, and removals require explicit confirmation and UAC elevation. It does not accept arbitrary Chocolatey command text.

## Safety

ToolBox launches only the fixed allowlist above. Other executables and scripts from the source tools folder are intentionally not packaged or exposed because some perform broad system/security changes or lack enough provenance for a safe default distribution.

Third-party tools have their own behavior and licenses. Review their documentation before redistribution or use in a production image.
