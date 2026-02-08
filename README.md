# OTA Updater CLI

A small OTA installer tool with slot-aware manifest selection, SHA-256 verification, and streaming install.

**Key features**
1. Slot-aware manifest selection (`slot-a` / `slot-b`)
2. SHA-256 verification of OTA bundle entries before install
3. Progress reporting (console or JSON file)
4. Archive, raw, and atomic file installers

## Build
```
./setup_and_build.sh
```

## Run
```
./build/flash_tool -i ota_sample/ota.tar
```

## Device Config
The installer reads a device config file to select the correct slot:

`/run/ota-updater/ota.conf`
```json
{
  "current_slot": "slot-a",
  "hw_compatibility": "jetson-orin-nano-devkit-nvme"
}
```

## Manifest Format
Slot sections are required. Each slot contains its own `components` list.

```json
{
  "version": "1.0.1",
  "hw_compatibility": "jetson-orin-nano-devkit-nvme",
  "slot-a": {
    "components": [
      {
        "name": "rootfs",
        "type": "archive",
        "filename": "core-image-full-cmdline.tar.gz",
        "install_to": "inactive-root-b",
        "size": 1073741881,
        "sha256": "..."
      }
    ]
  },
  "slot-b": {
    "components": [
      {
        "name": "rootfs",
        "type": "archive",
        "filename": "core-image-full-cmdline.tar.gz",
        "install_to": "inactive-root-a",
        "size": 1073741881,
        "sha256": "..."
      }
    ]
  }
}
```

## Generate a Sample OTA Bundle
`ota.sh` creates a large test bundle and a slot-based manifest.
```
./ota.sh
```

## Progress Output
Use `-p` to write JSON progress events to a file:
```
./build/flash_tool -i ota_sample/ota.tar -p progress.json
```

## Source Layout
Headers are grouped by domain:
1. `include/ota/` – OTA logic and installers
2. `include/io/` – I/O primitives
3. `include/crypto/` – SHA-256
4. `include/util/` – manifest, config, logger
5. `include/system/` – OS helpers
