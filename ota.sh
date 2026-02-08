set -euo pipefail

WORKDIR="$(pwd)/ota_sample"
ROOTFS_SIZE_MB="${ROOTFS_SIZE_MB:-1024}"
KERNEL_SIZE_MB="${KERNEL_SIZE_MB:-64}"
BOOTLOADER_SIZE_MB="${BOOTLOADER_SIZE_MB:-8}"
WIFI_SIZE_KB="${WIFI_SIZE_KB:-4}"
rm -rf "$WORKDIR"
mkdir -p "$WORKDIR/payload"

# 1) Make a rootfs directory with a large dummy payload then pack it as tar.gz
mkdir -p "$WORKDIR/rootfs/etc" "$WORKDIR/rootfs/bin"
echo "hello from rootfs" > "$WORKDIR/rootfs/etc/issue"
printf '#!/bin/sh\necho "hello from /bin/hello"\n' > "$WORKDIR/rootfs/bin/hello"
chmod +x "$WORKDIR/rootfs/bin/hello"

dd if=/dev/zero of="$WORKDIR/rootfs/large.bin" bs=1M count="$ROOTFS_SIZE_MB" status=none

tar -C "$WORKDIR/rootfs" -czf "$WORKDIR/payload/core-image-full-cmdline.tar.gz" .

# 2) Make a dummy "raw" blob (pretend kernel/initramfs image)
dd if=/dev/urandom of="$WORKDIR/payload/tegra-minimal-initramfs.cboot" bs=1M count="$KERNEL_SIZE_MB" status=none

# 3) Make a dummy bootloader capsule file (file component)
dd if=/dev/zero of="$WORKDIR/payload/tegra-bl.cap" bs=1M count="$BOOTLOADER_SIZE_MB" status=none

# 4) Make a dummy wifi config file (file component)
dd if=/dev/zero of="$WORKDIR/payload/wpa_supplicant.conf" bs=1K count="$WIFI_SIZE_KB" status=none
cat >> "$WORKDIR/payload/wpa_supplicant.conf" <<'EOF'
ctrl_interface=DIR=/run/wpa_supplicant GROUP=netdev
update_config=1
country=US
network={
    ssid="TestAP"
    psk="password123"
}
EOF
chmod 0600 "$WORKDIR/payload/wpa_supplicant.conf"

# 5) Compute sha256 for each artifact (these are the bytes stored inside ota.tar)
cd "$WORKDIR/payload"
SHA_ROOTFS="$(sha256sum core-image-full-cmdline.tar.gz | awk '{print $1}')"
SHA_KERNEL="$(sha256sum tegra-minimal-initramfs.cboot | awk '{print $1}')"
SHA_BL="$(sha256sum tegra-bl.cap | awk '{print $1}')"
SHA_WIFI="$(sha256sum wpa_supplicant.conf | awk '{print $1}')"

# 5.1) Compute uncompressed/original sizes for accurate progress
SIZE_ROOTFS="$(du -sb "$WORKDIR/rootfs" | awk '{print $1}')"
SIZE_KERNEL="$(stat -c %s "$WORKDIR/payload/tegra-minimal-initramfs.cboot")"
SIZE_BL="$(stat -c %s "$WORKDIR/payload/tegra-bl.cap")"
SIZE_WIFI="$(stat -c %s "$WORKDIR/payload/wpa_supplicant.conf")"

# 6) Write manifest.json (match filenames exactly)
cat > manifest.json <<EOF
{
  "version": "1.0.1",
  "hw_compatibility": "jetson-orin-nano-devkit-nvme",
  "components": [
    {
      "name": "rootfs",
      "type": "archive",
      "filename": "core-image-full-cmdline.tar.gz",
      "install_to": "inactive_app_partition",
      "size": $SIZE_ROOTFS,
      "sha256": "$SHA_ROOTFS"
    },
    {
      "name": "kernel",
      "type": "raw",
      "filename": "tegra-minimal-initramfs.cboot",
      "install_to": "inactive_kernel_partition",
      "size": $SIZE_KERNEL,
      "sha256": "$SHA_KERNEL"
    },
    {
      "name": "bootloader",
      "type": "file",
      "create-destination": true,
      "filename": "tegra-bl.cap",
      "path": "/tmp/ota_test/TEGRA_BL.Cap",
      "size": $SIZE_BL,
      "sha256": "$SHA_BL",
      "version": "36.4.4"
    },
    {
      "name": "wifi-config",
      "type": "file",
      "filename": "wpa_supplicant.conf",
      "path": "/tmp/ota_test/wpa_supplicant.conf",
      "permissions": "0600",
      "size": $SIZE_WIFI,
      "sha256": "$SHA_WIFI"
    }
  ]
}
EOF

# 7) Create ota.tar with manifest.json FIRST
tar -cf "$WORKDIR/ota.tar" \
  manifest.json \
  core-image-full-cmdline.tar.gz \
  tegra-minimal-initramfs.cboot \
  tegra-bl.cap \
  wpa_supplicant.conf

echo "Created: $WORKDIR/ota.tar"
echo "First entries:"
tar -tf "$WORKDIR/ota.tar" | head -n 5
