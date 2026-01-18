# Merged Binary Guide

## Overview

The OTGW-firmware build system now supports creating a **single merged binary** that contains both the firmware and LittleFS filesystem. This simplifies the flashing process by reducing it to a single file operation.

## Benefits

1. **Simpler flashing**: One file instead of two separate files
2. **Reduced errors**: No need to remember two different flash addresses
3. **Optional compression**: Gzip compression can reduce file size by ~40-60%
4. **Easier distribution**: Single file to download and share
5. **Backwards compatible**: Standard firmware + filesystem files are still created

## Building with Merged Binary

### Basic Merged Binary

```bash
python build.py --merged
```

This creates a merged binary in the `build/` directory:
- `OTGW-firmware-<version>-merged.bin` (or `OTGW-firmware-merged.bin` if `--no-rename` is used)

### Compressed Merged Binary

```bash
python build.py --merged --compress
```

This creates both the merged binary and a compressed version:
- `OTGW-firmware-<version>-merged.bin`
- `OTGW-firmware-<version>-merged.bin.gz` (compressed, ~40-60% smaller)

### Build Options

```bash
# Full build with merged binary
python build.py --merged

# Full build with compressed merged binary
python build.py --merged --compress

# Build without version renaming
python build.py --merged --no-rename

# Build firmware only, then create merged binary
python build.py --firmware --merged
```

## Flashing with Merged Binary

### Using flash_esp.py (Recommended)

The flash tool automatically detects and prioritizes merged binaries:

```bash
# Interactive mode (will auto-detect merged binary)
python flash_esp.py

# Explicitly specify merged binary
python flash_esp.py --merged build/OTGW-firmware-merged.bin

# Flash compressed merged binary (auto-decompresses)
python flash_esp.py --merged build/OTGW-firmware-merged.bin.gz

# Build with merged binary and flash
python flash_esp.py --build
# Note: Modify build_firmware() in flash_esp.py to call build.py with --merged flag
```

### Using esptool Directly

```bash
# Flash merged binary (uncompressed)
esptool.py --port <PORT> -b 460800 write_flash 0x0 OTGW-firmware-merged.bin

# Decompress first, then flash
gunzip -k OTGW-firmware-merged.bin.gz
esptool.py --port <PORT> -b 460800 write_flash 0x0 OTGW-firmware-merged.bin
```

**Important**: Merged binaries are flashed to address `0x0` only (not `0x0` and `0x200000`).

### Using Makefile

The Makefile doesn't currently support merged binaries, but you can still flash manually:

```bash
# Build using Makefile
make

# Create merged binary separately
python -c "from build import create_merged_binary; create_merged_binary('.', 'local', False)"

# Or use esptool directly
esptool.py --port /dev/ttyUSB0 -b 460800 write_flash 0x0 build/OTGW-firmware-merged.bin
```

## Technical Details

### Flash Layout

ESP8266 flash layout for OTGW-firmware (4MB):

```
0x000000 - Firmware (sketch)
0x200000 - Filesystem (LittleFS, 1MB)
```

The merged binary contains:
- Firmware at offset 0x0
- Filesystem at offset 0x200000
- Empty space (0xFF) between and after

### Merged Binary Creation

The merged binary is created using `esptool.py merge_bin`:

```bash
esptool.py --chip esp8266 merge_bin \
  -o OTGW-firmware-merged.bin \
  --flash_mode dio \
  --flash_freq 40m \
  --flash_size 4MB \
  0x0 OTGW-firmware.ino.bin \
  0x200000 OTGW-firmware.ino.littlefs.bin
```

### Compression

Gzip compression (level 9) typically achieves:
- Firmware: ~60% reduction
- Filesystem: ~40% reduction (depends on content)
- Overall merged binary: ~50% reduction

The flash tool (`flash_esp.py`) automatically decompresses `.gz` files before flashing.

### File Size Examples

Typical file sizes (may vary):

| File | Size |
|------|------|
| Firmware (.ino.bin) | ~450 KB |
| Filesystem (.littlefs.bin) | ~1 MB |
| Merged binary (.bin) | ~4 MB (padded to full flash size) |
| Compressed merged (.bin.gz) | ~600 KB |

## Workflow Integration

### For Developers

```bash
# Edit code
# ...

# Build and create merged binary
python build.py --merged

# Flash to device
python flash_esp.py --merged build/OTGW-firmware-merged.bin
```

### For CI/CD

Add to your build workflow:

```yaml
- name: Build firmware with merged binary
  run: python build.py --merged --compress

- name: Upload artifacts
  uses: actions/upload-artifact@v2
  with:
    name: firmware
    path: |
      build/OTGW-firmware-*-merged.bin
      build/OTGW-firmware-*-merged.bin.gz
```

### For End Users

Users can now download a single file and flash it:

```bash
# Download merged binary from release
wget https://github.com/rvdbreemen/OTGW-firmware/releases/download/v1.0.0/OTGW-firmware-1.0.0-merged.bin.gz

# Flash (auto-decompresses)
python flash_esp.py --merged OTGW-firmware-1.0.0-merged.bin.gz
```

## Troubleshooting

### "esptool not found"

Install esptool:

```bash
pip install esptool
```

The build script will attempt to install it automatically if missing.

### "Failed to create merged binary"

Ensure both firmware and filesystem binaries exist:

```bash
ls build/*.ino.bin
ls build/*.littlefs.bin
```

If missing, run a full build first:

```bash
python build.py
python build.py --merged
```

### Compression fails

The compression step requires Python's built-in `gzip` module (should be available in all Python 3.x installations). If it fails:

```bash
# Build without compression
python build.py --merged

# Compress manually
gzip -9 -k build/OTGW-firmware-merged.bin
```

### Flash verification fails

If flashing fails or the device doesn't boot:

1. Erase flash first: `python flash_esp.py --erase`
2. Try flashing at lower baud rate: `python flash_esp.py --baud 115200`
3. Verify the merged binary is valid:
   ```bash
   # Check file size (should be ~4MB or larger)
   ls -lh build/*-merged.bin
   ```

## Backwards Compatibility

The standard firmware and filesystem binaries are still created during the build process. Users can choose to:

- Use the merged binary (easier, single file)
- Use separate firmware and filesystem binaries (traditional method)

Both methods are fully supported and produce identical results on the device.

## FAQ

**Q: Is the merged binary larger than the separate files?**  
A: Yes, the uncompressed merged binary is padded to the full flash size (~4MB), but the compressed version (`.gz`) is typically smaller than downloading both separate files.

**Q: Can I use the merged binary with OTA updates?**  
A: The merged binary is primarily for initial flashing via serial. For OTA updates, the firmware still uses separate firmware and filesystem images as needed.

**Q: Does this work with all ESP8266 boards?**  
A: Yes, this works with any ESP8266 board that has 4MB flash (NodeMCU, Wemos D1 mini, etc.). The flash layout is standard.

**Q: Can I create a merged binary from an existing build?**  
A: Yes, if you already have the firmware and filesystem binaries in the `build/` directory, you can run:
   ```bash
   python build.py --merged --no-install-cli
   ```
   This will create the merged binary without rebuilding.

**Q: How do I flash only the filesystem using the merged binary?**  
A: You can't selectively flash parts of a merged binary. Use the separate filesystem binary for that:
   ```bash
   python flash_esp.py --filesystem build/OTGW-firmware.ino.littlefs.bin
   ```

## See Also

- [BUILD.md](BUILD.md) - General build instructions
- [FLASH_GUIDE.md](FLASH_GUIDE.md) - Flashing guide
- [ESP8266 esptool documentation](https://github.com/espressif/esptool)
