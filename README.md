# A CLI tool that helps Skytrack product

## Version 0.0.1
+ Support install rootfs ext4 format to specific paritition

## TODO:
1) Create a conf file that describes rootfs A/B partition and more needed field to support OTA

### CMAKE setup
1) Cross compile
```bash
cmake -S . -B build-aarch64   -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake   -DCMAKE_BUILD_TYPE=Release -DFLASH_TOOL_USE_SUBMODULES=ON
```
2) GTest
```bash
cmake -S . -B build -DFLASH_TOOL_USE_SUBMODULES=ON -DFLASH_TOOL_BUILD_TESTS=ON
```

### CMAKE build
`cmake --build build -j`

### CMAKE run test
`cmake --build build --target test`