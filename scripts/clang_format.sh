#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-check}" # check | fix

if ! command -v clang-format >/dev/null 2>&1; then
  echo "clang-format not found in PATH"
  exit 1
fi

mapfile -t FILES < <(find include src tests -type f \( -name '*.h' -o -name '*.hpp' -o -name '*.c' -o -name '*.cpp' \) | sort)

if [[ "${#FILES[@]}" -eq 0 ]]; then
  echo "No source files found."
  exit 0
fi

if [[ "$MODE" == "fix" ]]; then
  clang-format -i "${FILES[@]}"
  echo "Formatted ${#FILES[@]} files."
  exit 0
fi

if [[ "$MODE" != "check" ]]; then
  echo "Usage: $0 [check|fix]"
  exit 2
fi

DIFF_FOUND=0
for f in "${FILES[@]}"; do
  if ! diff -u "$f" <(clang-format "$f") >/dev/null; then
    echo "Needs format: $f"
    DIFF_FOUND=1
  fi
done

if [[ "$DIFF_FOUND" -ne 0 ]]; then
  echo "Formatting check failed. Run: ./scripts/clang_format.sh fix"
  exit 1
fi

echo "Formatting check passed."
