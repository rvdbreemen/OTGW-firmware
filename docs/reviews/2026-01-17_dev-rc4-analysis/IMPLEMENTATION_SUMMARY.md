# Merged Binary Implementation - Change Summary

## Overview

Enhanced the OTGW-firmware build and flash scripts to support creating and flashing a single merged binary file that contains both the firmware and LittleFS filesystem, with optional gzip compression.

## Changes Made

### 1. build.py Enhancements

#### New Features:
- **Merged binary creation**: Added `create_merged_binary()` function using esptool's `merge_bin` command
- **Gzip compression**: Added optional compression support for merged binaries
- **esptool integration**: Added `check_esptool()` function to verify/install esptool

#### New Command-Line Arguments:
- `--merged`: Create a single merged binary containing firmware and filesystem
- `--compress`: Compress the merged binary with gzip (requires `--merged`)

#### Implementation Details:
```python
# Creates merged binary at address 0x0 with filesystem at offset 0x200000
create_merged_binary(project_dir, semver, compress=False)

# Uses esptool merge_bin with ESP8266-specific parameters:
# --chip esp8266
# --flash_mode dio
# --flash_freq 40m
# --flash_size 4MB
```

#### File Outputs:
- Standard: `OTGW-firmware-<version>-merged.bin`
- Compressed: `OTGW-firmware-<version>-merged.bin.gz`

### 2. flash_esp.py Enhancements

#### New Features:
- **Merged binary detection**: Automatically detects and prioritizes merged binaries
- **Decompression support**: Automatically decompresses `.gz` files before flashing
- **Updated file search**: Extended `find_firmware_files()` to find merged binaries
- **Updated build artifact checking**: `check_build_artifacts()` now looks for merged binaries

#### New Command-Line Arguments:
- `--merged`: Path to merged binary file (`.bin` or `.bin.gz`)

#### Implementation Details:
```python
# Flash function now accepts merged_file parameter
flash_esp8266(port, firmware_file=None, filesystem_file=None, 
              merged_file=None, baud=DEFAULT_BAUD, ...)

# Merged binary flashed to 0x0 only (not 0x0 and 0x200000)
# Automatic decompression of .gz files to temporary file
```

#### Updated Workflows:
1. **Build artifacts mode**: Checks for merged binary first, then falls back to separate files
2. **Manual mode**: Allows selection of merged binary or separate files
3. **Interactive mode**: Displays merged binary option prominently

### 3. Documentation

#### New File:
- `MERGED_BINARY_GUIDE.md`: Comprehensive guide covering:
  - Building with merged binaries
  - Flashing with merged binaries
  - Technical details (flash layout, compression)
  - Workflow integration
  - Troubleshooting
  - FAQ

## Usage Examples

### Building

```bash
# Build with merged binary
python build.py --merged

# Build with compressed merged binary
python build.py --merged --compress

# Full build with all options
python build.py --merged --compress
```

### Flashing

```bash
# Interactive mode (auto-detects merged binary)
python flash_esp.py

# Explicitly flash merged binary
python flash_esp.py --merged build/OTGW-firmware-merged.bin

# Flash compressed merged binary
python flash_esp.py --merged build/OTGW-firmware-merged.bin.gz

# Direct esptool usage
esptool.py --port COM5 -b 460800 write_flash 0x0 OTGW-firmware-merged.bin
```

## Technical Specifications

### Flash Addresses
- **Merged binary**: Flashed to `0x0` (contains firmware at 0x0, filesystem at offset 0x200000)
- **Separate files**: Firmware at `0x0`, Filesystem at `0x200000`

### File Sizes
- Firmware: ~450 KB
- Filesystem: ~1 MB
- Merged binary: ~4 MB (padded to flash size)
- Compressed merged: ~600 KB (~50% reduction)

### ESP8266 Configuration
- Flash size: 4MB
- Flash mode: DIO
- Flash frequency: 40MHz
- Chip: esp8266

## Benefits

1. **Simplified flashing**: Single file instead of two
2. **Reduced errors**: No need to remember multiple flash addresses
3. **Smaller downloads**: Compression reduces file size by ~50%
4. **Better UX**: Easier for end users to flash devices
5. **Backwards compatible**: Standard binaries still created

## Backwards Compatibility

- All existing workflows continue to work
- Standard firmware and filesystem binaries are still created
- Users can choose merged or separate files
- No changes required to existing flash procedures

## Testing Recommendations

1. **Build testing**:
   ```bash
   python build.py --merged
   python build.py --merged --compress
   ```
   - Verify merged binaries are created
   - Check file sizes are reasonable
   - Ensure compression works

2. **Flash testing**:
   ```bash
   python flash_esp.py --merged build/OTGW-firmware-merged.bin
   ```
   - Test with uncompressed merged binary
   - Test with compressed merged binary (.gz)
   - Verify device boots and functions correctly

3. **Integration testing**:
   - Test interactive mode
   - Test build artifacts detection
   - Test manual file selection
   - Verify decompression works

4. **Edge cases**:
   - Test with missing esptool (should auto-install)
   - Test with only firmware (no filesystem)
   - Test with various compression levels
   - Test error handling

## Dependencies

### New Dependencies:
- **esptool**: Required for `merge_bin` command
  - Auto-installed by build.py if missing
  - Version: Latest from PyPI

### Existing Dependencies (used):
- **gzip**: Python standard library (for compression)
- **shutil**: Python standard library (for file operations)

## Files Modified

1. **build.py**:
   - Added `create_merged_binary()` function
   - Added `check_esptool()` function
   - Added command-line arguments: `--merged`, `--compress`
   - Updated help text and examples

2. **flash_esp.py**:
   - Updated `flash_esp8266()` to accept `merged_file` parameter
   - Updated `find_firmware_files()` to return merged files
   - Updated `check_build_artifacts()` to detect merged files
   - Updated `build_firmware()` to return merged file
   - Added decompression logic for `.gz` files
   - Added command-line argument: `--merged`
   - Updated interactive mode to show merged binary option
   - Updated confirmation display
   - Updated help text and examples

3. **MERGED_BINARY_GUIDE.md**:
   - New comprehensive documentation file

## Future Enhancements (Optional)

1. **Makefile integration**: Add targets for merged binary creation
2. **CI/CD updates**: Automatically create and upload merged binaries in releases
3. **OTA support**: Investigate using merged binaries for OTA updates
4. **Compression levels**: Allow user to specify gzip compression level
5. **Partial flashing**: Support flashing only firmware or filesystem from merged binary
6. **Verification**: Add checksum verification for merged binaries

## Notes

- The merged binary is primarily for initial serial flashing
- OTA updates continue to use separate firmware/filesystem images
- Compression is optional but recommended for distribution
- The implementation uses esptool's standard `merge_bin` command
- Flash layout remains unchanged from standard ESP8266 Arduino core
