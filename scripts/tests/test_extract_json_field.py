#!/usr/bin/env python3
"""
Algorithm validation for the hand-rolled inbound JSON field extractor that
replaces ArduinoJson's deserializeJson in extractJsonField() (TASK-886, full
ADR-141 revert).

There is NO host C compiler in this environment (only the xtensa cross-toolchain,
which cannot run on the host), so this Python implementation mirrors the C
control flow 1:1. It validates the ALGORITHM; the C transcription is then
syntax-checked by the real esp32 build and exercised on-device by the ~18 POST
callers. The single property that matters and was broken in the pre-ArduinoJson
scanner: a key-looking substring INSIDE a string value must never be matched as
a key. That is guaranteed here by skipping every value wholesale (strings,
nested objects, nested arrays) instead of substring-scanning.

Run: python3 scripts/tests/test_extract_json_field.py
"""

import sys


# ---- 1:1 mirror of the C helpers in jsonStuff.ino -------------------------

def _hex(c):
    if '0' <= c <= '9': return ord(c) - ord('0')
    if 'a' <= c <= 'f': return ord(c) - ord('a') + 10
    if 'A' <= c <= 'F': return ord(c) - ord('A') + 10
    return -1


def _put_utf8(out, cp):
    # append codepoint as UTF-8 bytes; mirrors xjf_putUtf8 (BMP, 1-3 bytes)
    if cp < 0x80:
        out.append(cp)
    elif cp < 0x800:
        out.append(0xC0 | (cp >> 6))
        out.append(0x80 | (cp & 0x3F))
    else:
        out.append(0xE0 | (cp >> 12))
        out.append(0x80 | ((cp >> 6) & 0x3F))
        out.append(0x80 | (cp & 0x3F))


def _read_string(s, i, out, cap):
    """
    s[i] must be the opening '"'. If out is not None, append unescaped bytes,
    TRUNCATING to cap-1 (strlcpy semantics, matching the old ArduinoJson path
    via strlcpy(result, v.as<const char*>(), resultSize)). Truncation does NOT
    fail the parse: scanning continues to the closing quote so the caller's
    position stays well-formed. A partial multibyte \\u sequence is dropped
    rather than written half (no invalid UTF-8 tail). Returns index PAST the
    closing '"', or None only if unterminated/malformed. Mirrors xjf_readString.
    """
    if i >= len(s) or s[i] != '"':
        return None
    i += 1
    full = False  # result buffer reached cap-1; keep scanning, stop appending
    while i < len(s) and s[i] != '"':
        if s[i] == '\\':
            if i + 1 >= len(s):
                return None
            e = s[i + 1]
            if e == '"': c = ord('"'); i += 2
            elif e == '\\': c = ord('\\'); i += 2
            elif e == '/': c = ord('/'); i += 2
            elif e == 'b': c = ord('\b'); i += 2
            elif e == 'f': c = ord('\f'); i += 2
            elif e == 'n': c = ord('\n'); i += 2
            elif e == 'r': c = ord('\r'); i += 2
            elif e == 't': c = ord('\t'); i += 2
            elif e == 'u':
                if i + 5 >= len(s):
                    return None
                h0, h1, h2, h3 = _hex(s[i+2]), _hex(s[i+3]), _hex(s[i+4]), _hex(s[i+5])
                if min(h0, h1, h2, h3) < 0:
                    return None
                cp = (h0 << 12) | (h1 << 8) | (h2 << 4) | h3
                need = 1 if cp < 0x80 else (2 if cp < 0x800 else 3)
                if out is not None and not full:
                    if len(out) + need <= cap - 1:
                        _put_utf8(out, cp)
                    else:
                        full = True
                i += 6
                continue
            else:
                c = ord(e); i += 2
            if out is not None and not full:
                if len(out) + 1 <= cap - 1:
                    out.append(c)
                else:
                    full = True
            continue
        # raw byte
        if out is not None and not full:
            if len(out) + 1 <= cap - 1:
                out.append(ord(s[i]) & 0xFF)
            else:
                full = True
        i += 1
    if i >= len(s) or s[i] != '"':
        return None  # unterminated
    return i + 1


def _skip_ws(s, i):
    while i < len(s) and s[i] in ' \t\n\r':
        i += 1
    return i


def _skip_value(s, i):
    """s[i] at '{' or '['. Skip the balanced container respecting strings.
    Returns index past the closer, or None. Mirrors xjf_skipValue."""
    depth = 0
    while i < len(s):
        c = s[i]
        if c == '"':
            i = _read_string(s, i, None, 0)
            if i is None:
                return None
            continue
        if c == '{' or c == '[':
            depth += 1
            i += 1
            continue
        if c == '}' or c == ']':
            depth -= 1
            i += 1
            if depth == 0:
                return i
            continue
        i += 1
    return None


def extract_json_field(json_str, key, cap=64):
    """
    Mirrors extractJsonFieldImpl(): extract the TOP-LEVEL scalar field `key`
    from the flat JSON object `json_str`. Returns the value string, or None if
    absent / explicit null / value is a container / malformed-at-key-position.
    `cap` is the C result buffer size (NUL-terminated -> cap-1 usable bytes).
    """
    s = json_str
    i = 0
    n = len(s)
    while i < n and s[i] != '{':
        i += 1
    if i >= n or s[i] != '{':
        return None
    i += 1
    while True:
        i = _skip_ws(s, i)
        if i >= n or s[i] == '}':
            return None
        if s[i] != '"':
            return None  # malformed at key position
        kbuf = []
        after = _read_string(s, i, kbuf, 48)
        if after is None:
            return None
        i = _skip_ws(s, after)
        if i >= n or s[i] != ':':
            return None
        i = _skip_ws(s, i + 1)
        match = (bytes(kbuf).decode('utf-8', 'replace') == key)
        if i < n and s[i] == '"':
            if match:
                out = []
                np = _read_string(s, i, out, cap)
                if np is None:
                    return None
                return bytes(out).decode('utf-8', 'replace')
            np = _read_string(s, i, None, 0)
            if np is None:
                return None
            i = np
        elif i < n and (s[i] == '{' or s[i] == '['):
            np = _skip_value(s, i)
            if np is None:
                return None
            if match:
                return None  # container, not a scalar
            i = np
        else:
            start = i
            while i < n and s[i] not in ',}\t \n\r':
                i += 1
            token = s[start:i]
            if match:
                if token == 'null':
                    return None
                return token[:cap - 1]
        i = _skip_ws(s, i)
        if i < n and s[i] == ',':
            i += 1
            continue
        return None  # '}' or end or junk -> not found


# ---- test table -----------------------------------------------------------

# (description, json, key, expected-or-None)
CASES = [
    ("simple string",          '{"command":"PR=A"}',                       "command",    "PR=A"),
    ("key-in-value not matched",'{"other":"command","command":"X"}',       "command",    "X"),
    ("escaped quote in value",  '{"a":"b\\"command\\":z","command":5}',    "command",    "5"),
    ("integer",                 '{"value":42}',                            "value",      "42"),
    ("negative float",          '{"value":-3.5}',                          "value",      "-3.5"),
    ("exponent number",         '{"value":1.5e3}',                         "value",      "1.5e3"),
    ("bool true",               '{"flag":true}',                           "flag",       "true"),
    ("bool false",              '{"flag":false}',                          "flag",       "false"),
    ("explicit null -> absent", '{"value":null}',                          "value",      None),
    ("missing key",             '{"a":1}',                                 "value",      None),
    ("whitespace everywhere",   '{   "value"  :   42   }',                 "value",      "42"),
    ("target is last",          '{"a":1,"b":2,"value":3}',                 "value",      "3"),
    ("target is first",         '{"value":1,"a":2}',                       "value",      "1"),
    ("nested obj value skipped",'{"obj":{"value":9},"value":7}',           "value",      "7"),
    ("matched value is object", '{"value":{"x":1}}',                       "value",      None),
    ("array value skipped",     '{"arr":[1,2,{"value":3}],"value":8}',     "value",      "8"),
    ("escapes in value",        '{"label":"a\\"b\\\\c\\nd"}',              "label",      'a"b\\c\nd'),
    ("unicode bmp diacritic",   '{"label":"caf\\u00e9"}',                  "label",      "café"),
    ("comma inside string val", '{"label":"a,b","x":1}',                   "x",          "1"),
    ("brace inside string val", '{"label":"a}b","x":1}',                   "x",          "1"),
    ("colon inside string val", '{"label":"a:b","x":1}',                   "x",          "1"),
    ("empty object",            '{}',                                      "value",      None),
    ("unterminated string val", '{"command":"abc',                        "command",    None),
    ("leading junk before {",   'garbage {"command":"ok"}',                "command",    "ok"),
    # realistic captured bodies
    ("webhook event",           '{"event":"boiler_temp","value":45.5,"duration_s":300}', "event",      "boiler_temp"),
    ("webhook value",           '{"event":"boiler_temp","value":45.5,"duration_s":300}', "value",      "45.5"),
    ("webhook duration",        '{"event":"boiler_temp","value":45.5,"duration_s":300}', "duration_s", "300"),
    ("settings POST name",      '{"name":"settingHostname","value":"OTGW-test"}',        "name",       "settingHostname"),
    ("settings POST value",     '{"name":"settingHostname","value":"OTGW-test"}',        "value",      "OTGW-test"),
    ("sat simulate area",       '{"area":"woonkamer","sensor":"AA:BB:CC"}',              "area",       "woonkamer"),
    ("sat label outside_temp",  '{"outside_temp":-2.5,"flow_temp":40,"label":"west"}',   "outside_temp","-2.5"),
]


def main():
    fails = 0
    for desc, js, key, expected in CASES:
        try:
            got = extract_json_field(js, key)
        except Exception as ex:  # a parser must never throw on adversarial input
            print(f"FAIL  {desc:32}  EXCEPTION {ex!r}")
            fails += 1
            continue
        ok = (got == expected)
        if not ok:
            print(f"FAIL  {desc:32}  key={key!r}  got={got!r}  expected={expected!r}")
            fails += 1
        else:
            print(f"pass  {desc:32}  -> {got!r}")

    # truncation / overflow safety: value longer than the result buffer must be
    # truncated to cap-1 (strlcpy semantics), succeed, never overflow, never throw.
    long_val = "x" * 200
    got = extract_json_field('{"k":"' + long_val + '"}', "k", cap=16)
    if got is None or len(got) != 15:
        print(f"FAIL  truncation bound (strlcpy)      got len={None if got is None else len(got)} (want 15)")
        fails += 1
    else:
        print(f"pass  truncation bound (strlcpy)       -> len {len(got)} == 15")

    print()
    if fails:
        print(f"RESULT: {fails} FAIL")
        sys.exit(1)
    print(f"RESULT: all {len(CASES) + 1} pass")


if __name__ == "__main__":
    main()
