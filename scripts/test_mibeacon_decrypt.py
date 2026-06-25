#!/usr/bin/env python3
"""Known-answer test for the encrypted Xiaomi MiBeacon (v4/v5) AES-CCM recipe
used by the firmware in SATble.ino::mibeaconDecryptInPlace (TASK-930 Phase 2).

This pins the exact nonce / AAD / key-length / packet-geometry the C code
implements, so a future change to that understanding fails here instead of
silently auth-failing on a real sensor (the device field-test cannot run on a
bench without a real encrypted Mijia + its bindkey).

Vector: xiaomi-ble test suite `test_Xiaomi_LYWSD03MMC_encrypted`
(https://github.com/Bluetooth-Devices/xiaomi-ble, tests/test_parser.py).
The library asserts humidity == 46 (it truncates 46.7 -> 46).

Run:  python scripts/test_mibeacon_decrypt.py
Requires the `cryptography` package (AES-CCM). Exits non-zero on mismatch.
"""
import sys

# --- the known-answer vector --------------------------------------------------
# Raw 0xFE95 service-data bytes (== the parser's `data`):
RAW = bytes([0x58, 0x58, 0x5B, 0x05, 0x50, 0xF4, 0x83, 0x02, 0x38, 0xC1, 0xA4,
             0x95, 0xEF, 0x58, 0x76, 0x3C, 0x26, 0x00, 0x00, 0x97, 0xE2, 0xAB, 0xB5])
BINDKEY = bytes.fromhex("e9ea895fac7cca6d30532432a516f3a8")  # 16 bytes
MAC = "A4:C1:38:02:83:F4"
EXPECT_PLAINTEXT = bytes.fromhex("061002d301")  # obj 0x1006 humidity = 0x01D3/10 = 46.7
EXPECT_HUMIDITY_INT = 46


def recipe_decrypt(raw: bytes, key: bytes) -> bytes:
    """Reproduce SATble.ino::mibeaconDecryptInPlace exactly."""
    from cryptography.hazmat.primitives.ciphers.aead import AESCCM
    n = len(raw)
    assert n == 19 or 22 <= n <= 24, f"unexpected size {n}"
    cipher_pos = 5 if n == 19 else 11
    data_size = (n - 12) if n == 19 else (n - 18)
    ct = raw[cipher_pos:cipher_pos + data_size]
    ext_ctr = raw[n - 7:n - 4]
    tag = raw[n - 4:]
    onair_mac = raw[5:11]          # bit4 set here; == BLE source addr LSB-first
    nonce = onair_mac + raw[2:5] + ext_ctr   # 6 + (prodid2+counter1) + extctr3 = 12
    assert len(nonce) == 12
    # AESCCM.decrypt raises InvalidTag if key/nonce/aad/tag are wrong.
    return AESCCM(key, tag_length=4).decrypt(nonce, ct + tag, b"\x11")


def parse_humidity(plaintext: bytes):
    i = 0
    while i + 3 <= len(plaintext):
        oid = plaintext[i] | (plaintext[i + 1] << 8)
        ln = plaintext[i + 2]
        v = plaintext[i + 3:i + 3 + ln]
        i += 3 + ln
        if oid == 0x1006 and len(v) >= 2:
            return (v[0] | (v[1] << 8)) / 10.0
    return None


def firmware_walker_offset(decrypted_full: bytes):
    """Mirror SATble.ino::parseBLEMiBeaconFormat offset derivation on the
    in-place-decrypted FULL buffer (bit3 cleared, bit4 preserved). Returns
    (pos, humidity_or_None) — proves the bit-derived offset matches cipherPos
    so the walker lands on the plaintext objects, not the (unencrypted) MAC.
    This refutes the 'bit4=0 -> walker misparses MAC' concern: bit4 stays set
    for the 22-24B geometry, so pos == 11."""
    fc = decrypted_full[0] | (decrypted_full[1] << 8)
    assert (fc & 0x08) == 0, "bit3 must be cleared post-decrypt"
    pos = 5
    if fc & 0x10:
        pos += 6                      # mac-included (must stay set for 22-24B)
    if fc & 0x20:
        pos += 1
        if decrypted_full[pos - 1] & 0x20:
            pos += 1
    return pos, parse_humidity(decrypted_full[pos:])


def main() -> int:
    fc = RAW[0] | (RAW[1] << 8)
    assert (fc >> 3) & 1, "vector must be encrypted (frctrl bit3)"
    assert (fc >> 12) == 5, "vector is MiBeacon v5"
    assert (fc & 0x10) != 0, "vector has mac-included (bit4) -> 22-24B geometry, cipher@11"
    # MAC layout sanity: raw[5:11] reversed == display MAC
    assert ":".join(f"{b:02X}" for b in reversed(RAW[5:11])) == MAC, "MAC layout"
    pt = recipe_decrypt(RAW, BINDKEY)
    if pt != EXPECT_PLAINTEXT:
        print(f"FAIL: plaintext {pt.hex()} != {EXPECT_PLAINTEXT.hex()}")
        return 1
    hum = parse_humidity(pt)
    if hum is None or int(hum) != EXPECT_HUMIDITY_INT:
        print(f"FAIL: humidity {hum} (int) != {EXPECT_HUMIDITY_INT}")
        return 1
    # Reconstruct the in-place-decrypted full buffer and confirm the firmware
    # walker's bit-derived offset lands on the plaintext objects (pos 11), not
    # the unencrypted MAC at [5:11].
    cipher_pos = 11  # this 23B vector
    full = bytearray(RAW)
    full[0] &= ~0x08                              # clear encryption bit (as the firmware does)
    full[cipher_pos:cipher_pos + len(pt)] = pt    # overwrite ciphertext with plaintext
    pos, whum = firmware_walker_offset(bytes(full))
    if pos != cipher_pos:
        print(f"FAIL: walker offset {pos} != cipherPos {cipher_pos} (bit4 handling)")
        return 1
    if whum is None or int(whum) != EXPECT_HUMIDITY_INT:
        print(f"FAIL: walker humidity {whum} != {EXPECT_HUMIDITY_INT}")
        return 1
    print(f"PASS: decrypt+auth OK plaintext={pt.hex()} humidity={hum}; "
          f"walker offset={pos} (==cipherPos, bit4 preserved) humidity={whum}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except ImportError:
        print("SKIP: `cryptography` not installed (pip install cryptography)")
        sys.exit(0)
