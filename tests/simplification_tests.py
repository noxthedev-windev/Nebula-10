#!/usr/bin/env python
"""Policy for the maintained native Store and BGRT-free package."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
package = (ROOT / "scripts/package-release.sh").read_text(encoding="utf-8")
toolbox = (ROOT / "src/n10toolbox.cpp").read_text(encoding="utf-8")
setup = (ROOT / "src/setup.cpp").read_text(encoding="utf-8")

assert "add_executable(N10Store" in cmake, "native Store source must be built"
assert "src/n10store.cpp" in cmake, "Store source is not wired into the build"
assert "add_executable(NebulaBGRT" not in cmake, "BGRT controller must not be built"
assert "NebulaBGRT" not in toolbox, "BGRT must not be exposed in ToolBox"
assert 'L"NebulaBGRT.exe"' not in setup[setup.index("std::vector<std::wstring> files(") : setup.index("bool write_integrity_state")], "BGRT must not be an installed payload"
for forbidden in ("NebulaBGRT/Runtime", "NebulaBGRT/Source", "NebulaBGRT.exe"):
    assert forbidden not in package, f"BGRT leaked into package script: {forbidden}"

print("simplification_tests: PASS (maintained native Store, no distributed BGRT)")
