.PHONY: configure build test package

configure:
	cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

build: configure
	cmake --build build --config Release --parallel 6

package: build
	"C:/Program Files/Git/bin/bash.exe" scripts/package-release.sh

test: package
	ctest --test-dir build --output-on-failure
	python tests/smoke_tests.py "dist/.package-stage/Nebula10-Fix-1.1"
	python tests/feature_tests.py "dist/.package-stage/Nebula10-Fix-1.1"

	python tests/store_tests.py "dist/.package-stage/Nebula10-Fix-1.1"
	python tests/package_tests.py "dist/.package-stage/Nebula10-Fix-1.1"
	python tests/simplification_tests.py
