# Report: OTGW-firmware PIC Flashing Crash & Analysis

## 1. Issue Summary
**Symptom:** The ESP8266 would crash (Exception 9 or 28) or fail to update when identifying or flashing the PIC firmware in Release Candidate 3. This occurred during `GetVersion` calls or the initial phase of the flash process.

**Root Cause:**
The firmware processes `.hex` files by loading chunks of data into a 256-byte buffer (`datamem`). This buffer is initialized with `0xFF` and populated with binary data from the hex file. Crucially, this data is **not** a valid C-string (it lacks a null terminator).

The original code used `strstr_P` (and similar logic) to search for the "OpenTherm Gateway" banner string inside this buffer. Since `strstr` relies on finding a null terminator to know when to stop reading, it would read past the end of the `datamem` buffer into protected memory, causing a fatal hardware exception.

## 2. Fix Implementation
We implemented a **Robust "Sliding Window" Search Algorithm** in both `src/libraries/OTGWSerial/OTGWSerial.cpp` and `versionStuff.ino`.

### Technical Details of the Fix:
1.  **Strict Bounds Checking:** The search loop now iterates strictly from index `0` to `datasize - bannerLen`. This mathematically guarantees that no memory read ever exceeds the buffer boundary.
2.  **Safe Comparison:** Replaced `strstr` with `strncmp_P`. This compares exactly `N` characters and stops, ignoring whether the surrounding data is null-terminated or garbage.
3.  **File Stream Correction:** Added `hexfd.seek(0)` in `OTGWSerial.cpp`. The validation routine reads the file to the end to check checksums; without this rewind, the actual programming phase tried to read from the end of the file and failed immediately.

## 3. Comparative Analysis of Flashing Algorithms

We compared the `OTGW-firmware` implementation against the `otmonitor` (Tcl) reference and Microchip datasheets for PIC16F88/PIC16F1847.

### Architecture Comparison

| Feature | **OTGW-firmware (Native C++)** | **otmonitor (Tcl Script)** |
| :--- | :--- | :--- |
| **Execution** | Embedded on ESP8266 (constrained RAM) | running on PC (unlimited RAM) |
| **File Handling** | Streamed from LittleFS on-the-fly | Loads full file into RAM |
| **Recovery** | **Has dedicated "Failsafe" state** | Relies on standard erase/write |
| **Robustness** | High (handles power loss) | Medium (dependent on PC connection) |

### Key Findings
1.  **Failsafe Mechanism:** The OTGW-firmware `OTGWSerial` library contains a sophisticated `FWSTATE_PREP` state. Before erasing the main application, it injects a "Recovery Vector" (assembly jumps) into the first block of flash.
    *   *Significance:* If power fails during the update, the PIC will reset, hit this vector, and safely re-enter the bootloader instead of bricking. This is a superior design feature not explicitly present in the simpler Tcl implementation.
2.  **Memory Management:** The OTGW-firmware implementation is highly optimized for the ESP8266's limited RAM (`~40KB` free). It processes the update in small chunks (rows), whereas `otmonitor` parses the whole file structure in memory.
3.  **Correctness:** The `PicInfo` structures in `OTGWSerial.cpp` correctly match the Microchip datasheets for both 16F88 and 16F1847 regarding Erase/Write block sizes (Flash latches).

## 4. Conclusion
The flashing implementation in `OTGW-firmware` is superior for the embedded use case due to its specific Failsafe/Recovery logic. The recent crashes were an implementation bug in string handling, not a flaw in the flashing protocol itself.

**Recommendation:** The current codebase, with the applied "Sliding Window" and "File Seek" fixes, is the most robust solution. No logic needs to be ported from `otmonitor`.

## 5. WebUI Progress Bar Issue (Resolved)
**Symptom:** The user reported that the flashing progress bar in the WebUI remained empty (0%) despite WebSocket traffic indicating activity.

**Root Cause:**
Analysis of the `OTGW-Core.ino` file revealed a typo in the JSON formatted strings generated during the update process. 
The code used: `snprintf_P(..., PSTR("{\")percent\":%d}"), ...)` 
This resulted in a JSON payload with the key named `")percent"` (e.g., `{"percent": 10}`). The WebUI JavaScript expects the key `"percent"`. 
Since `msg.hasOwnProperty('percent')` returned false, the UI ignored the update messages.

**Resolution:**
The malformed format strings were corrected in `OTGW-Core.ino` (lines ~2089, ~2091, ~2101) to remove the stray parenthesis. The backend now sends valid JSON payloads: `{"percent":...}` and `{"result":...}`.
