#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build}"

run_clang_tidy() {
  if ! command -v clang-tidy >/dev/null 2>&1; then
    echo "[scan] clang-tidy not found, skipping."
    return 0
  fi

  if [[ ! -f "$BUILD_DIR/compile_commands.json" ]]; then
    echo "[scan] compile_commands.json missing in $BUILD_DIR."
    echo "[scan] Configure with: cmake -S . -B $BUILD_DIR -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    return 1
  fi

  mapfile -t CPP_FILES < <(find src tests -type f \( -name '*.cpp' -o -name '*.c' \) | sort)
  if [[ "${#CPP_FILES[@]}" -eq 0 ]]; then
    echo "[scan] No C/C++ source files found for clang-tidy."
    return 0
  fi

  echo "[scan] Running clang-tidy on ${#CPP_FILES[@]} files..."
  clang-tidy -p "$BUILD_DIR" "${CPP_FILES[@]}"
}

run_cppcheck() {
  if ! command -v cppcheck >/dev/null 2>&1; then
    echo "[scan] cppcheck not found, skipping."
    return 0
  fi

  echo "[scan] Running cppcheck..."
  cppcheck \
    --enable=warning,style,performance,portability \
    --std=c++23 \
    --suppress=missingIncludeSystem \
    -I include \
    src tests
}

run_clang_tidy
run_cppcheck
echo "[scan] Static scan completed."
