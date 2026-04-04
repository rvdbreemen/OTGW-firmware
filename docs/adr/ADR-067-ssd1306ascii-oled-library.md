# ADR-067: SSD1306Ascii Text-Only OLED Library

**Status:** Accepted
**Date:** 2026-04-04
**Decision Maker:** User: Robert van den Breemen

## Context

### Problem Statement
The OLED display module (OLED.ino) used the Adafruit SSD1306 library which requires a 1024-byte RAM framebuffer (128x64 / 8 = 1024 bytes). On ESP8266 with only ~40KB usable RAM, this is ~2.5% of total RAM consumed even when the display content is purely text-based.

### Background
The OLED display shows 4 pages of text information: temperatures, system status, network info, and firmware version. The only non-text elements were a 16x16 flame bitmap icon and a horizontal separator line. The Adafruit library also pulls in the Adafruit GFX dependency, adding flash overhead.

### Constraints
- ESP8266 has ~40KB usable RAM - every KB matters
- Display content is 95% text with simple formatting
- Must support runtime I2C detection (display may not be present)
- Must work on both ESP8266 and ESP32-S3 platforms
- Library must be available in arduino-cli and PlatformIO registries

## Decision

Replace Adafruit SSD1306 + Adafruit GFX with the SSD1306Ascii library (greiman/SSD1306Ascii, pinned at version 1.3.5). This is a text-only library that writes directly to the display without a RAM framebuffer.

### Why This Choice
- Saves 1024 bytes RAM (no framebuffer)
- Removes two dependencies (Adafruit SSD1306 + Adafruit GFX), replaces with one (SSD1306Ascii)
- The display content is text-only in practice - the flame bitmap and separator line were cosmetic
- SSD1306Ascii supports F() flash strings, matching the project's PROGMEM requirements
- The library is mature (v1.3.5), well-maintained, and widely used

### Implementation Summary
- OLED.ino rewritten to use `SSD1306AsciiWire` class with `System5x7` font
- Row-based positioning (8 rows of 8px each) instead of pixel coordinates
- Flame bitmap replaced with `*` text character
- Horizontal separator replaced with row of `-` characters
- Display on/off via direct SSD1306 commands (0xAE/0xAF)
- `oledBuf[22]` static module-level buffer for snprintf_P formatting

## Alternatives Considered

### Alternative 1: Adafruit SSD1306 with Dynamic Allocation
**Description:** Keep Adafruit library but allocate the display object (and its framebuffer) dynamically only when I2C probe finds a display.

**Pros:**
- Retains bitmap and graphics capability
- No RAM cost when display is absent

**Cons:**
- Still costs 1KB heap when display is present
- Heap allocation can fragment memory over time
- Two library dependencies remain

**Why Not Chosen:** Still consumes 1KB when display is connected. The graphics capabilities (bitmap, line drawing) are not worth the RAM cost for text-only content.

### Alternative 2: U8g2 Library
**Description:** Full-featured monochrome display library with multiple buffer modes including page-buffer (128 bytes) and full-buffer (1024 bytes).

**Pros:**
- Page-buffer mode uses only 128 bytes RAM
- Supports graphics, fonts, and Unicode
- Very comprehensive library

**Cons:**
- Large flash footprint (~20-30KB depending on features)
- Complex API for simple text display
- Over-engineered for this use case

**Why Not Chosen:** Flash overhead too large for ESP8266 (already at 66% flash usage). Complexity not justified for text-only display.

### Alternative 3: No OLED Support on ESP8266
**Description:** Disable OLED entirely on ESP8266 (`HAS_OLED_CAPABLE=0`), only support on ESP32.

**Pros:**
- Zero RAM/flash cost on ESP8266

**Cons:**
- Removes functionality for users who connect an OLED to ESP8266 hardware
- ESP8266 Nodoshop board has I2C pins available for OLED

**Why Not Chosen:** Users may want OLED on ESP8266; removing capability is unnecessary when SSD1306Ascii provides it at near-zero cost.

## Consequences

### Positive
- **RAM savings:** 1024 bytes freed on ESP8266 when display is connected, 0 bytes when absent
- **Fewer dependencies:** 2 libraries removed, 1 added (net reduction of 1 dependency)
- **Simpler code:** Direct text output without framebuffer management
- **PROGMEM compliant:** F() strings work natively with the library's print() method

### Negative
- **No graphics:** Cannot draw bitmaps, lines, or geometric shapes
  - Accepted: display content is text-only; flame icon replaced with `*` character
- **Font limitations:** Only bitmap fonts included with library (no TrueType)
  - Accepted: System5x7 is adequate for 128x64 status display
- **No display() call:** Text appears immediately when printed (no double-buffering)
  - Accepted: at 1 Hz refresh with clear() first, no visible flicker

### Risks & Mitigation
- **Risk:** Future requirement for graphics on OLED
  **Mitigation:** SSD1306Ascii can be replaced with U8g2 (page-buffer mode) if graphics become needed. The API is different but the display logic would remain similar.

## Implementation Notes

### Key Files Affected
- `src/OTGW-firmware/OLED.ino` - Complete rewrite using SSD1306AsciiWire API
- `build.py` - Replaced Adafruit SSD1306@2.5.13 + Adafruit GFX@1.11.11 with SSD1306Ascii@1.3.5
- `platformio.ini` - Same library swap in lib_deps

### API Mapping

| Adafruit SSD1306 | SSD1306Ascii |
|---|---|
| `clearDisplay()` + `display()` | `clear()` |
| `setCursor(x_px, y_px)` | `setRow(row)` + `setCol(col_px)` |
| `setTextSize(n)` | `set1X()` / `set2X()` |
| `print(F("text"))` | `print(F("text"))` |
| `ssd1306_command(0xAE)` | `ssd1306WriteCmd(0xAE)` |
| `drawBitmap(...)` | Not available (text-only) |
| `drawLine(...)` | Not available (use `-` characters) |

## Related Decisions

- **Related to:** ADR-004 (Static Buffer Allocation) - RAM conservation strategy
- **Related to:** ADR-009 (PROGMEM String Literals) - SSD1306Ascii supports F() strings
- **Related to:** ADR-063 (OTGW32 Hardware Support) - OLED used on both platforms

## References

- SSD1306Ascii library: https://github.com/greiman/SSD1306Ascii
- Library version pinned: 1.3.5

## Timeline

- **2026-04-04:** Accepted and implemented
