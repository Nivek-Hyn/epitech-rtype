#!/usr/bin/env bash
# Run local checks: formatting, build without warnings, static analysis and unit tests.
#
#   scripts/check.sh                                   # every test
#   scripts/check.sh '^(Registry|FixedTimestep)\.'     # only the tests matching a regular expression

set -uo pipefail
cd "$(dirname "$0")/.."

BUILD_DIR=build
TEST_FILTER="${1:-}"
PYTHON_EXECUTABLE="${PYTHON:-python3}"
failed=0
build_ready=0

step() { printf '\n== %s\n' "$1"; }

step "Formatting"
if find engine common server client tests \( -name '*.cpp' -o -name '*.hpp' \) -print0 |
    xargs -0 clang-format --dry-run --Werror; then
    echo "OK"
else
    echo "Run: find engine common server client tests -name '*.[ch]pp' | xargs clang-format -i"
    failed=1
fi

step "Build"
if cmake -S . -B "$BUILD_DIR" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DRTYPE_BUILD_TESTS=ON &&
    cmake --build "$BUILD_DIR" -j 4; then
    echo "OK"
    build_ready=1
else
    failed=1
fi

step "Static analysis"
if [ "$build_ready" -eq 1 ]; then
    if ! "$PYTHON_EXECUTABLE" scripts/lint.py --build-dir "$BUILD_DIR"; then
        failed=1
    fi
else
    echo "Skipped: fix the build errors above first"
fi

step "Tests"
test_arguments=()
if [ -n "$TEST_FILTER" ]; then
    test_arguments=(-R "$TEST_FILTER")
fi
if [ "$build_ready" -eq 1 ]; then
    if ! ctest --test-dir "$BUILD_DIR" --output-on-failure --no-tests=error "${test_arguments[@]}"; then
        failed=1
    fi
else
    echo "Skipped: fix the build errors above first"
fi

step "Result"
if [ "$failed" -eq 0 ]; then
    echo "All checks passed"
else
    echo "Some checks failed"
fi
exit "$failed"
