#!/usr/bin/env python
"""Policy for the simplified fixed Store payload and BGRT-free package."""
from pathlib import Path
import hashlib

ROOT = Path(__file__).resolve().parents[1]
EXPECTED_STORE = "9ca2fcaeab4388125efa3863e79d3bc5ffa13f4621fa8617e6f9c315e352724f"
store = ROOT / "payload" / "N10Store.exe"
assert store.is_file(), "preserved N10Store payload missing"
assert hashlib.sha256(store.read_bytes()).hexdigest() == EXPECTED_STORE, "old N10Store EXE changed"

cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
package = (ROOT / "scripts/package-release.sh").read_text(encoding="utf-8")
toolbox = (ROOT / "src/n10toolbox.cpp").read_text(encoding="utf-8")
setup = (ROOT / "src/setup.cpp").read_text(encoding="utf-8")

assert "add_executable(N10Store" not in cmake, "Store must be copied from the fixed old EXE"
assert "payload/N10Store.exe" in cmake + package, "fixed Store payload is not wired into build/package"
assert "add_executable(NebulaBGRT" not in cmake, "BGRT controller must not be built"
assert "NebulaBGRT" not in toolbox, "BGRT must not be exposed in ToolBox"
assert 'L"NebulaBGRT.exe"' not in setup[setup.index("std::vector<std::wstring> files()") : setup.index("bool write_integrity_state")], "BGRT must not be an installed payload"
for forbidden in ("NebulaBGRT/Runtime", "NebulaBGRT/Source", "NebulaBGRT.exe"):
    assert forbidden not in package, f"BGRT leaked into package script: {forbidden}"

print("simplification_tests: PASS (fixed old Store payload, no distributed BGRT)")
