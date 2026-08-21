# Licenses and attribution

Nebula10 Native original C++ code is provided under the MIT License in `LICENSE`.

The packaged `NebulaBGRT\Runtime` and `NebulaBGRT\Source` content is a modified distribution of HackBGRT and remains licensed under GNU GPL v3. The full GPL license, upstream README, source, change history, and `NEBULA_NOTICE.md` are retained under `NebulaBGRT\Source`; runtime copies of the license and notice are also included.

The user-facing `NebulaBGRT.exe` controller is original Nebula10 native code. The private `NebulaBGRT\Runtime\engine.exe` is the modified upstream-compatible engine and is not represented as original Microsoft software or an unmodified upstream release.

Prebuilt EFI payloads were imported unchanged from the matching official HackBGRT v2.6.0 release because Clang/`gnu-efi` were unavailable on this host. Exact URL and SHA-256 provenance are recorded in `NEBULA_NOTICE.md`. Included shim binaries retain their own copyright notice.
