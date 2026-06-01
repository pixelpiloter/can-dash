#!/usr/bin/env bash
# tools/run_cppcheck.sh
# Lightweight static analysis for Layer 1 (shm) + Layer 2 (runtime) +
# tests/. UI (Qt) and generated code are excluded — they pull in Qt headers
# not present on the build host for lint purposes.

set -euo pipefail

cppcheck --version >/dev/null || {
    echo "cppcheck not installed (apt install cppcheck) — skipping"
    exit 0
}

# Build a file list to feed cppcheck
mapfile -t files < <(
    find src/layer1 src/layer2 tests -type f \( -name '*.cpp' -o -name '*.c' -o -name '*.h' -o -name '*.hpp' \) 2>/dev/null
)

if [ "${#files[@]}" -eq 0 ]; then
    echo "run_cppcheck: no files to check"
    exit 0
fi

cppcheck \
    --enable=warning,style,performance,portability \
    --inline-suppr \
    --suppress=missingIncludeSystem \
    --suppress=missingInclude \
    --suppress=unusedFunction \
    --suppress=unusedStructMember \
    --suppress=unreadVariable \
    --std=c++17 \
    --language=c++ \
    --error-exitcode=1 \
    --quiet \
    "${files[@]}"
