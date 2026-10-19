#!/usr/bin/env bash
# Run locally the same checks as the CI: formatting, build without warnings, unit tests.
#
#   scripts/check.sh                                   # every test
#   scripts/check.sh '^(Registry|FixedTimestep)\.'     # only the tests matching a regular expression

set -uo pipefail
cd "$(dirname "$0")/.."

BUILD_DIR=build
TEST_FILTER="${1:-}"
failed=0

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
if cmake -S . -B "$BUILD_DIR" > /dev/null && cmake --build "$BUILD_DIR" -j 4; then
    echo "OK"
else
    failed=1
fi

step "Tests"
if [ "$failed" -eq 0 ]; then
    if ! ctest --test-dir "$BUILD_DIR" --output-on-failure ${TEST_FILTER:+-R "$TEST_FILTER"}; then
        failed=1
    fi
else
    echo "Skipped: fix the errors above first"
fi

step "Result"
if [ "$failed" -eq 0 ]; then
    echo "All checks passed"
else
    echo "Some checks failed"
fi
exit "$failed"
