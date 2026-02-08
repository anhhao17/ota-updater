#!/usr/bin/env bash
set -euo pipefail

PROFILE_HOST="$PWD/conan/profiles/linux-aarch64-gcc"
PROFILE_BUILD="$PWD/conan/profiles/linux-x86_64-release"

BUILD_DIR="build/aarch64"
HOST_TEST_BUILD_DIR="build/host-tests"

conan profile detect --force >/dev/null 2>&1 || true

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

conan install . \
  -pr:b "$PROFILE_BUILD" \
  -pr:h "$PROFILE_HOST" \
  -of "$BUILD_DIR" \
  -s:h build_type=Release \
  -o:h "libarchive/*:with_acl=False" \
  --build=missing

TOOLCHAIN="$PWD/build/aarch64/build/Release/generators/conan_toolchain.cmake"

cmake -S . -B build/aarch64 \
  -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
  -DCMAKE_BUILD_TYPE=Release

cmake --build "$BUILD_DIR" -j

# Cross builds do not register gtest cases in this project, so run tests in a native host build.
rm -rf "$HOST_TEST_BUILD_DIR"

conan install . \
  -pr:b "$PROFILE_BUILD" \
  -pr:h "$PROFILE_BUILD" \
  -of "$HOST_TEST_BUILD_DIR" \
  -s:h build_type=Release \
  -o:h "libarchive/*:with_acl=False" \
  --build=missing

HOST_TEST_TOOLCHAIN="$PWD/$HOST_TEST_BUILD_DIR/build/Release/generators/conan_toolchain.cmake"

cmake -S . -B "$HOST_TEST_BUILD_DIR" \
  -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$HOST_TEST_TOOLCHAIN" \
  -DCMAKE_BUILD_TYPE=Release \
  -DFLASH_TOOL_BUILD_TESTS=ON

cmake --build "$HOST_TEST_BUILD_DIR" -j
ctest --test-dir "$HOST_TEST_BUILD_DIR" --output-on-failure

# test on device
./ota.sh

${HOST_TEST_BUILD_DIR}/flash_tool -i ota_sample/ota.tar
# clean up
rm -rf ./ota_sample
