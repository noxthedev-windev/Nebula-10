# Nebula10 security model

Nebula10 does not alter, replace, redirect, disable, or take ownership of Windows consent infrastructure. It does not provide an arbitrary privileged command runner.

## UserAuth boundary

- `n10toolbox` launches `NebulaUserAuth.exe` with `CREATE_NEW_CONSOLE`, producing a real separate Windows console.
- UserAuth displays the exact action and accepts only literal `APPROVE`; it never asks for or stores credentials.
- The Windows service pipe uses `PIPE_REJECT_REMOTE_CLIENTS`.
- Its explicit ACL grants access to LocalSystem, administrators, and local interactive desktop users—not network clients.
- The service impersonates the pipe client and verifies membership in the Windows Interactive SID before processing a request.
- Input is bounded to a short UTF-16 action ID.
- The service and shared policy compile in exactly four actions: `LONG_PATHS_ON/OFF` and `OEM_BRANDING_ON/OFF`.
- Unknown text cannot be interpreted as a path, executable, command line, script, registry key, or shell command.
- Every processed request is logged under `%ProgramData%\Nebula10\service.log` with its local client PID.

The service permits terminal-only approval after installation because its powers are intentionally narrow. Adding arbitrary process launch, arbitrary registry paths, file writes, or command execution would turn it into an unsafe elevation bypass and must not be done.

## Recovery

Current-user settings preserve original existence, type, and data before mutation. `n10toolbox rollback` restores them. Machine ON actions save the original machine value; the matching OFF action restores it. Nebula Setup separately backs up the pre-install OEM `Manufacturer` and `Model` values before applying the Nebula identity and restores them during uninstall. The custom `HKLM\SOFTWARE\Nebula10\Identity` key contains only Nebula-owned metadata and is removed on uninstall.

NebulaBGRT changes the boot chain and carries separate risks involving Secure Boot, TPM measurements, BitLocker, Windows Hello/PIN, and anti-cheat software. Setup selects the Nebula boot logo by default but still requires a separate typed confirmation after presenting those risks; `--no-bgrt` opts out. Successful installation is tracked, and uninstall restores the boot path before deleting runtime files. If restoration fails, uninstall stops and preserves recovery components. Users interact only with the native `NebulaBGRT.exe` command controller; the GPL engine is private package runtime content and no setup-named BGRT executable is exposed. Keep recovery media and recovery keys available. The normal test suite never installs services, writes settings, changes PATH/shortcuts, or modifies boot configuration.

## ForceOwn boundary

`n10forceown.exe` is intentionally separate from the Nebula UserAuth service. It cannot submit file paths or commands to the LocalSystem broker. The tool accepts only path arguments and fixed switches, previews its targets, requires confirmation, and uses genuine Windows UAC before invoking absolute System32 paths for `takeown.exe` and `icacls.exe`. Drive roots and protected Windows/application-data trees are denied unless the caller adds `--allow-system` and accepts the stronger warning. Operations and native exit codes are logged.

Taking ownership can break application servicing or Windows security descriptors. Use it for files you own or deliberately need to recover—not as a way to patch Microsoft binaries or bypass product security.

## N10Store boundary

N10Store contains a compiled, curated mapping from short slugs to exact winget package IDs. User input cannot become an executable name, package source, or arbitrary command. Package operations call winget directly with `--exact` and `--source winget`; `--dry-run` performs no network or package action. Publisher trust, manifests, hashes, and installer behavior remain governed by Windows Package Manager and the selected third-party package publisher.
