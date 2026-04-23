# OTGW-firmware host tests

Small host-compilable unit tests for pure logic that does not depend on the
Arduino runtime. They let us validate byte-manipulation code on a laptop
without flashing an ESP8266.

Full integration testing still happens on the device itself; these tests
cover isolated, deterministic functions only.

## Current tests

| File | What it covers |
| --- | --- |
| `test_dallas_address.cpp` | `getDallasAddress()` hex-string conversion for Dallas DS18B20 ROM codes |

## Building and running

Any C++17 host compiler works (gcc, clang, MSVC). The tests stub
`PROGMEM` and `pgm_read_byte()` so no Arduino headers are required.

### g++ / clang++ (Linux, macOS, MSYS2, Git Bash with MinGW)

```bash
g++ -std=c++17 tests/test_dallas_address.cpp -o tests/test_dallas_address.out
./tests/test_dallas_address.out
echo "exit=$?"   # 0 on success, 1 on failure
```

`clang++` is a drop-in replacement:

```bash
clang++ -std=c++17 tests/test_dallas_address.cpp -o tests/test_dallas_address.out
```

### MSVC (Developer Command Prompt)

```bat
cl /std:c++17 /EHsc tests\test_dallas_address.cpp /Fe:tests\test_dallas_address.exe
tests\test_dallas_address.exe
```

### Expected output

```
=== Dallas Address Conversion Test ===
standard address                 expected=28FF641E8216C3A1 got=28FF641E8216C3A1 PASS
leading zero bytes               expected=0102030405060708 got=0102030405060708 PASS
all zeros                        expected=0000000000000000 got=0000000000000000 PASS
all 0xFF                         expected=FFFFFFFFFFFFFFFF got=FFFFFFFFFFFFFFFF PASS
length check                     expected=16 got=16 PASS
mixed nibbles                    expected=AABBCCDDEEFF1122 got=AABBCCDDEEFF1122 PASS
NUL terminator at index 16       PASS
=== ALL TESTS PASSED (failures=0) ===
```

Exit code is `0` on pass, `1` if any check fails.

## Artifacts

Build output (`*.out`, `*.exe`) is ignored by the existing repo `.gitignore`
patterns for compiled binaries. If you need to keep them out explicitly,
add `tests/*.out` and `tests/*.exe` to your local ignore list.

## Adding a new host test

1. Write a single self-contained `.cpp` file under `tests/`.
2. Stub any Arduino-only macros/types at the top (`PROGMEM`, `pgm_read_byte`,
   `F()`, `PSTR()`, etc.). Keep stubs minimal and explicitly marked.
3. Copy the function under test verbatim, or `#include` a header if the
   firmware source already has a clean host-compatible seam.
4. Implement a small `main()` that runs the cases, prints pass/fail per
   case, and returns non-zero if any case fails.
5. Add a row to the table above and a build command if it differs.

Do not add anything that requires `millis()`, `delay()`, `Serial`, network
stacks, or actual peripherals. Those belong on the device.
