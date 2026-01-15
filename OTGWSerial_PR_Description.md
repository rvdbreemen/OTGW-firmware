# Fixes for ESP8266 Crash and Update Failure in OTGWSerial

This Pull Request addresses two critical issues identified when running the OTGW firmware on ESP8266 (specifically with the `LittleFS` filesystem and recent Arduino Core versions). These issues caused the firmware to crash during PIC updates or fail to perform the update entirely.

## 1. Fix: Buffer Overread Crash (Exception 9/28)

### The Issue
In `OTGWUpgrade::readHexFile`, the code used `strstr_P` to search for the firmware banner string within the `datamem` buffer.

```cpp
char *s = strstr_P((char *)datamem + ptr, banner1);
```

The `datamem` buffer is initialized with `0xFF` (via `memset(..., -1, ...))` and populated with data from the hex file. It is **not** guaranteed to be null-terminated. `strstr_P` will continue reading memory until it finds a null terminator (`\0`). If the `datamem` buffer (and the memory immediately following it) does not contain a null byte, `strstr_P` reads into out-of-bounds memory, resulting in a fatal crash (Exception 9 or 28) on the ESP8266.

### The Fix
Replaced the unsafe `strstr_P` usage with a bounded loop using `strncmp_P`. This implementation respects the `info.datasize` limit and ensures no out-of-bounds reads occur.

```cpp
// NEW SAFE IMPLEMENTATION
size_t bannerLen = sizeof(banner1) - 1;
bool match = false;
if (ptr + bannerLen <= info.datasize) {
     if (strncmp_P((char *)datamem + ptr, banner1, bannerLen) == 0) {
         match = true;
     }
}
```

## 2. Fix: File Pointer Reset for Flashing

### The Issue
The `readHexFile` function parses the entire hex file to validate checksums, determine the PIC model, and extract version information. At the end of this function, the file stream cursor (`hexfd`) is positioned at the **End Of File (EOF)**.

When the state machine subsequently attempts to perform the actual upgrade (resetting the PIC and reading code blocks), it fails to read any data because the file pointer was never reset to the beginning of the file.

### The Fix
Added a `seek(0)` call before returning from `readHexFile` (or before the state machine starts consuming the file again) to ensure the file stream is ready for reading from the start.

```cpp
// Added to readHexFile before returning success
if (hexfd) hexfd.seek(0);
```

## Impact
Without these fixes, the PIC firmware update feature is non-functional on current builds, leading to device reboots or failed updates. Validated that these changes restore full update functionality.
