# anatomy.md

> Auto-maintained by OpenWolf. Last scanned: 2026-07-02T05:19:53.083Z
> Files: 542 tracked | Anatomy hits: 0 | Misses: 0

## ../../../Claude/Projects/Kluis/06-claude/

- `kb-calibrate-set.json` (~1348 tok)

## ../LLmWiki-KennisBank/

- `CHANGELOG.md` — Changelog (~9526 tok)
- `CONFIGURATION.md` — CONFIGURATION (~9715 tok)

## ../LLmWiki-KennisBank/scripts/

- `_hooks_manifest.py` — _hooks_manifest.py - de canonieke lijst van KennisBank-hooks. (~348 tok)
- `_rank.py` — _rank.py - retrieval-scoring: relevance x recency x importance + graafbuur. (~1630 tok)
- `_usage.py` — _usage.py - usage-telemetrie voor de retrieval-feedbackloop. (~1477 tok)
- `kb-calibrate.py` — kb-calibrate.py - drempel-kalibratie voor het actieve embeddingmodel. (~2178 tok)
- `kb-recall.py` — kb-recall.py - geheugen-recall over kb-index.db (lokaal, fail-soft). (~1802 tok)
- `kb-retrieve.py` — UserPromptSubmit hook: inject relevant KennisBank wiki snippets for a prompt. (~2529 tok)
- `kb-usage-scan.py` — SessionEnd hook: sluit de retrieval-feedbackloop. (~1063 tok)
- `stale-check.py` — parse_frontmatter, parse_date, load_sessie_dates, mentions_article (~1620 tok)

## ../LLmWiki-KennisBank/tests/

- `test_kb_calibrate.py` — Tests voor scripts/kb-calibrate.py - drempel-kalibratie (pure functies). (~821 tok)
- `test_usage.py` — Tests voor scripts/_usage.py en kb-usage-scan.py - de retrieval-feedbackloop. (~1447 tok)

## ./

- `.codex` (~0 tok)
- `.gitattributes` — Git attributes (~139 tok)
- `.gitignore` — Git ignore rules (~677 tok)
- `.gitmodules` (~164 tok)
- `.mcp.json` (~56 tok)
- `.tmp_esp32_build.log` (~11793 tok)
- `AGENTS.md` — OTGW-firmware: Codex Agent Instructions (~10728 tok)
- `AUTHORS` — Authors (~19 tok)
- `build.bat` (~1617 tok)
- `build.py` — Colors: asset_slug, disable, print_step, print_success + 7 more (~32176 tok)
- `build.sh` (~1576 tok)
- `CHANGELOG.md` — Change log (~8375 tok)
- `CLAUDE.md` — OpenWolf (~10033 tok)
- `config.py` — Base Paths (~268 tok)
- `evaluate.py` — drift: strip_css_comments, strip_js_comments, extract_classes_from_html, extract_classes_from_js + 5 more (~47028 tok)
- `flash_esp.py` — Colors: disable, print_header, print_success, print_error + 10 more (~10963 tok)
- `flash_otgw.bat` (~2804 tok)
- `flash_otgw.sh` — flash_otgw.sh - Self-contained ESP flash tool for OTGW-firmware (Linux/macOS) (~3385 tok)
- `LICENSE` — Project license (~295 tok)
- `Makefile` — Make build targets (~2037 tok)
- `partitions_otgw_esp32_combo.csv` — OTGW-firmware ESP32-S3 COMBO partition table — single app (no OTA), 4MB flash (~364 tok)
- `partitions_otgw_esp32.csv` — OTGW-firmware ESP32-S3 partition table — single app (no OTA), 4MB flash (~275 tok)
- `platformio.ini` — Declares used (~3999 tok)
- `README.md` — Project documentation (~5272 tok)

## .build-venv/

- `.gitignore` — Git ignore rules (~19 tok)
- `pyvenv.cfg` (~93 tok)

## .build-venv/Lib/site-packages/

- `_cffi_backend.cp314-win_amd64.pyd` (~48038 tok)
- `distutils-precedence.pth` (~41 tok)
- `reedsolo.py` — -*- coding: utf-8 -*- (~20281 tok)

## .build-venv/Lib/site-packages/_distutils_hack/

- `__init__.py` — don't import any costly modules (~1930 tok)
- `override.py` (~13 tok)

## .build-venv/Lib/site-packages/_yaml/

- `__init__.py` — This is a stub package designed to roughly emulate the _yaml (~401 tok)

## .build-venv/Lib/site-packages/bitarray-3.8.2.dist-info/

- `INSTALLER` (~2 tok)
- `METADATA` — Declares which (~9413 tok)
- `RECORD` (~447 tok)
- `top_level.txt` (~3 tok)
- `WHEEL` (~27 tok)

## .build-venv/Lib/site-packages/bitarray-3.8.2.dist-info/licenses/

- `LICENSE` — Project license (~656 tok)

## .build-venv/Lib/site-packages/bitarray/

- `__init__.py` — frozenbitarray: test (~664 tok)
- `__init__.pyi` — This stub, as well as util.pyi, are tested with Python 3.10 and mypy 1.11.2 (~1539 tok)
- `_bitarray.cp314-win_amd64.pyd` (~18152 tok)
- `_util.cp314-win_amd64.pyd` (~12326 tok)
- `bitarray.h` — Declares char (~3194 tok)
- `py.typed` (~0 tok)
- `pythoncapi_compat.h` — Header file providing new C API functions to old Python versions. (~21888 tok)
- `test_281.pickle` (~118 tok)
- `test_bitarray.py` — bitarray is published under the PSF license. (~53559 tok)
- `test_util.py` — bitarray is published under the PSF license. (~31647 tok)
- `util.py` — bitarray is published under the PSF license. (~6240 tok)
- `util.pyi` (~809 tok)

## .build-venv/Lib/site-packages/bitstring-4.4.0.dist-info/

- `INSTALLER` (~2 tok)
- `METADATA` — Declares as (~1497 tok)
- `RECORD` (~868 tok)
- `top_level.txt` (~3 tok)
- `WHEEL` (~25 tok)

## .build-venv/Lib/site-packages/bitstring-4.4.0.dist-info/licenses/

- `LICENSE` — Project license (~297 tok)

## .build-venv/Lib/site-packages/bitstring/

- `__init__.py` — returns: bytealigned, bytealigned, lsb0, lsb0 + 19 more (~4194 tok)
- `__main__.py` — main (~474 tok)
- `array_.py` — Array: itemsize, trailing_bits, dtype, dtype + 2 more (~10318 tok)
- `bitarray_.py` — BitArray: fromstring, copy (~6643 tok)
- `bits.py` — Declares is (~21420 tok)
- `bitstore_bitarray_helpers.py` — bin2bitstore, hex2bitstore, oct2bitstore, int2bitstore + 2 more (~902 tok)
- `bitstore_bitarray.py` — _BitStore: from_zeros, from_bin, from_bytes, frombuffer + 35 more (~3358 tok)
- `bitstore_common_helpers.py` — str_to_bitstore, bitstore_from_token, ue2bitstore, se2bitstore + 12 more (~1866 tok)
- `bitstore_tibs_helpers.py` — bin2bitstore, hex2bitstore, oct2bitstore, int2bitstore + 2 more (~668 tok)
- `bitstore_tibs.py` — ConstBitStore: join, from_zeros, from_tibs, from_bytes + 43 more (~4509 tok)
- `bitstream.py` — ConstBitStream: append, overwrite, find, rfind + 3 more (~8404 tok)
- `bitstring_options.py` — Options: using_rust_core, mxfp_overflow, mxfp_overflow, lsb0 + 4 more (~1252 tok)
- `dtypes.py` — Dtype: scaled_get_fn, wrapper, scaled_set_fn, wrapper + 20 more (~4828 tok)
- `exceptions.py` — Declares Error (~158 tok)
- `fp8.py` — Binary8Format: decompress_luts, create_luts, float_to_int8, createLUT_for_float16_to_binary8 + 2 more (~1077 tok)
- `helpers.py` — offset_slice_indices_lsb0, tidy_input_string (~449 tok)
- `luts.py` — This file is generated by generate_luts.py. DO NOT EDIT. (~7735 tok)
- `methods.py` — pack (~1260 tok)
- `mxfp.py` — MXFPFormat: round_to_nearest_ties_to_even, decompress_luts, create_luts, float_to_int + 4 more (~2588 tok)
- `py.typed` (~0 tok)
- `utils.py` — A token name followed by optional : then an integer number (~2700 tok)

## .build-venv/Lib/site-packages/cffi-2.0.0.dist-info/

- `entry_points.txt` (~19 tok)
- `INSTALLER` (~2 tok)
- `METADATA` (~701 tok)
- `RECORD` (~872 tok)
- `top_level.txt` (~5 tok)
- `WHEEL` (~27 tok)

## .build-venv/Lib/site-packages/cffi-2.0.0.dist-info/licenses/

- `AUTHORS` (~56 tok)
- `LICENSE` — Project license (~300 tok)

## .build-venv/Lib/site-packages/cffi/

- `__init__.py` (~146 tok)
- `_cffi_errors.h` — ifndef CFFI_MESSAGEBOX (~1117 tok)
- `_cffi_include.h` — *******  CPython-specific section  ********* (~4302 tok)
- `_embedding.h` — ** Support code for embedding **** (~5368 tok)
- `_imp_emulation.py` — get_suffixes, find_module, load_dynamic (~846 tok)
- `_shimmed_dist_utils.py` (~638 tok)
- `api.py` — FFI: cdef, are, embedding_api, dlopen + 8 more (~12049 tok)
- `backend_ctypes.py` — CTypesType: cmp, set_ffi, load_library, new_void_type + 1 more (~12130 tok)
- `cffi_opcode.py` — CffiOp: as_c_expr, as_python_bytes, format_four_bytes (~1638 tok)
- `commontypes.py` — resolve_common_type, win_common_types (~802 tok)
- `cparser.py` — specifier: source, replace, replace, replace_keeping_newlines + 2 more (~12798 tok)
- `error.py` — Declares FFIError (~251 tok)
- `ffiplatform.py` — URL configuration (~1024 tok)
- `lock.py` — allocate_lock: acquire (~214 tok)
- `model.py` — type qualifiers (~6228 tok)
- `parse_c_type.h` — Declares char (~1708 tok)
- `pkgconfig.py` — pkg-config, https://www.freedesktop.org/wiki/Software/pkg-config/ integration for cffi (~1250 tok)
- `recompiler.py` — GlobalExpr: as_c_expr, as_python_expr, as_c_expr, as_python_expr + 12 more (~18717 tok)
- `setuptools_ext.py` — URL configuration (~2689 tok)
- `vengine_cpy.py` — DEPRECATED: implementation for ffi.verify() (~12538 tok)
- `vengine_gen.py` — DEPRECATED: implementation for ffi.verify() (~7697 tok)
- `verifier.py` — DEPRECATED: implementation for ffi.verify() (~3195 tok)

## .build-venv/Lib/site-packages/click-8.4.2.dist-info/

- `INSTALLER` (~2 tok)
- `METADATA` — Declares toolkit (~699 tok)
- `RECORD` (~676 tok)
- `WHEEL` (~22 tok)

## .build-venv/Lib/site-packages/click-8.4.2.dist-info/licenses/

- `LICENSE.txt` (~369 tok)

## .build-venv/Lib/site-packages/click/

- `__init__.py` (~1324 tok)
- `_compat.py` — URL configuration (~5394 tok)
- `_termui_impl.py` — _BufferedTextPagerStream: render_finish, pct, time_per_iteration, eta + 10 more (~9066 tok)
- `_textwrap.py` — TextWrapper: extra_indent, indent_only (~1792 tok)
- `_utils.py` — Declares import (~285 tok)
- `_winconsole.py` — This module is based on the excellent work by Adam Bartoš who (~2441 tok)
- `core.py` — ParameterSource: batch, augment_usage_errors, iter_params_for_processing, sort_key (~40176 tok)
- `decorators.py` — to: pass_context, new_func, pass_obj, new_func + 24 more (~5632 tok)
- `exceptions.py` — ClickException: format_message, show, show, format_message + 5 more (~3390 tok)
- `formatting.py` — Can force a width.  This is used by the test system (~2984 tok)
- `globals.py` — get_current_context, get_current_context, get_current_context, push_context + 2 more (~550 tok)
- `parser.py` — _Option: takes_value, process, process, add_option + 2 more (~5444 tok)
- `py.typed` (~0 tok)
- `shell_completion.py` — CompletionItem: shell_complete, func_name, source_vars, source + 9 more (~6463 tok)
- `termui.py` — hidden_prompt_func, prompt, prompt_func, confirm + 4 more (~9490 tok)
- `testing.py` — EchoingStdin: read, read1, readline, readlines + 14 more (~7560 tok)
- `types.py` — ParamTypeInfoDict: to_info_dict, get_metavar, get_missing_message, convert + 11 more (~12797 tok)
- `utils.py` — URL configuration (~5983 tok)

## .build-venv/Lib/site-packages/colorama-0.4.6.dist-info/

- `INSTALLER` (~2 tok)
- `METADATA` — multiple: all (~4574 tok)
- `RECORD` (~580 tok)
- `WHEEL` (~28 tok)

## .build-venv/Lib/site-packages/colorama-0.4.6.dist-info/licenses/

- `LICENSE.txt` (~373 tok)

## .build-venv/Lib/site-packages/colorama/

- `__init__.py` (~76 tok)
- `ansi.py` — AnsiCodes: code_to_chars, set_title, clear_screen, clear_line + 5 more (~721 tok)
- `ansitowin32.py` — StreamWrapper: write, isatty, closed, should_wrap + 10 more (~3180 tok)
- `initialise.py` — reset_all, init, deinit, just_fix_windows_console + 3 more (~950 tok)
- `win32.py` — from winbase.h (~1766 tok)
- `winterm.py` — WinColor: get_osfhandle, get_attrs, set_attrs, reset_all + 11 more (~2039 tok)

## .build-venv/Lib/site-packages/colorama/tests/

- `__init__.py` (~22 tok)
- `ansi_test.py` — Test file (~812 tok)
- `ansitowin32_test.py` — Tests: closed_shouldnt_raise_on_closed_stream, closed_shouldnt_raise_on_detached_stream, reset_all_shouldnt_raise_on_closed_orig_stdout, wrap_shoul... (~3051 tok)
- `initialise_test.py` — Test file (~1926 tok)
- `isatty_test.py` — Tests: TTY, nonTTY, withPycharm, withPycharmTTYOverride + 3 more (~534 tok)
- `utils.py` — StreamTTY: isatty, isatty, osname, replace_by + 2 more (~309 tok)
- `winterm_test.py` — Test file (~1060 tok)

## .build-venv/Lib/site-packages/cryptography-49.0.0.dist-info/

- `INSTALLER` (~2 tok)
- `METADATA` (~1159 tok)
- `RECORD` (~4669 tok)
- `WHEEL` (~26 tok)

## .build-venv/Lib/site-packages/cryptography-49.0.0.dist-info/licenses/

- `LICENSE` — Project license (~53 tok)
- `LICENSE.APACHE` — Declares name (~3030 tok)
- `LICENSE.BSD` (~409 tok)

## .build-venv/Lib/site-packages/cryptography-49.0.0.dist-info/sboms/

- `cryptography-rust.cyclonedx.json` (~13027 tok)
- `sbom.json` (~350 tok)

## .build-venv/Lib/site-packages/cryptography/

- `__about__.py` — This file is dual licensed under the terms of the Apache License, Version (~128 tok)
- `__init__.py` — This file is dual licensed under the terms of the Apache License, Version (~104 tok)
- `exceptions.py` — This file is dual licensed under the terms of the Apache License, Version (~311 tok)
- `fernet.py` — This file is dual licensed under the terms of the Apache License, Version (~1990 tok)
- `py.typed` (~0 tok)
- `utils.py` — This file is dual licensed under the terms of the Apache License, Version (~1224 tok)

## .build-venv/Lib/site-packages/cryptography/hazmat/

- `__init__.py` — This file is dual licensed under the terms of the Apache License, Version (~130 tok)
- `_oid.py` — This file is dual licensed under the terms of the Apache License, Version (~5109 tok)

## .build-venv/Lib/site-packages/cryptography/hazmat/asn1/

- `__init__.py` — This file is dual licensed under the terms of the Apache License, Version (~222 tok)
- `asn1.py` — This file is dual licensed under the terms of the Apache License, Version (~5472 tok)

## .build-venv/Lib/site-packages/cryptography/hazmat/backends/

- `__init__.py` — This file is dual licensed under the terms of the Apache License, Version (~104 tok)

## .build-venv/Lib/site-packages/cryptography/hazmat/backends/openssl/

- `__init__.py` — This file is dual licensed under the terms of the Apache License, Version (~88 tok)
- `backend.py` — This file is dual licensed under the terms of the Apache License, Version (~3026 tok)

## .build-venv/Lib/site-packages/cryptography/hazmat/bindings/

- `__init__.py` — This file is dual licensed under the terms of the Apache License, Version (~52 tok)

## .build-venv/Lib/site-packages/cryptography/hazmat/bindings/_rust/

- `__init__.pyi` — This file is dual licensed under the terms of the Apache License, Version (~610 tok)
- `_openssl.pyi` — This file is dual licensed under the terms of the Apache License, Version (~61 tok)
- `asn1.pyi` — This file is dual licensed under the terms of the Apache License, Version (~95 tok)
- `declarative_asn1.pyi` — This file is dual licensed under the terms of the Apache License, Version (~1031 tok)
- `exceptions.pyi` — This file is dual licensed under the terms of the Apache License, Version (~171 tok)
- `ocsp.pyi` — This file is dual licensed under the terms of the Apache License, Version (~1072 tok)
- `pkcs12.pyi` — This file is dual licensed under the terms of the Apache License, Version (~428 tok)
- `pkcs7.pyi` — This file is dual licensed under the terms of the Apache License, Version (~427 tok)
- `test_support.pyi` — This file is dual licensed under the terms of the Apache License, Version (~202 tok)
- `x509.pyi` — This file is dual licensed under the terms of the Apache License, Version (~2722 tok)

## .build-venv/Lib/site-packages/cryptography/hazmat/bindings/_rust/openssl/

- `__init__.pyi` — This file is dual licensed under the terms of the Apache License, Version (~414 tok)
- `aead.pyi` — This file is dual licensed under the terms of the Apache License, Version (~1222 tok)
- `ciphers.pyi` — This file is dual licensed under the terms of the Apache License, Version (~351 tok)
- `cmac.pyi` — This file is dual licensed under the terms of the Apache License, Version (~151 tok)
- `dh.pyi` — This file is dual licensed under the terms of the Apache License, Version (~418 tok)
- `dsa.pyi` — This file is dual licensed under the terms of the Apache License, Version (~347 tok)
- `ec.pyi` — This file is dual licensed under the terms of the Apache License, Version (~451 tok)
- `ed25519.pyi` — This file is dual licensed under the terms of the Apache License, Version (~142 tok)
- `ed448.pyi` — This file is dual licensed under the terms of the Apache License, Version (~138 tok)
- `hashes.pyi` — This file is dual licensed under the terms of the Apache License, Version (~287 tok)
- `hmac.pyi` — This file is dual licensed under the terms of the Apache License, Version (~188 tok)
- `hpke.pyi` — This file is dual licensed under the terms of the Apache License, Version (~778 tok)
- `kdf.pyi` — This file is dual licensed under the terms of the Apache License, Version (~1766 tok)
- `keys.pyi` — This file is dual licensed under the terms of the Apache License, Version (~244 tok)
- `mldsa.pyi` — This file is dual licensed under the terms of the Apache License, Version (~287 tok)
- `mlkem.pyi` — This file is dual licensed under the terms of the Apache License, Version (~223 tok)
- `poly1305.pyi` — This file is dual licensed under the terms of the Apache License, Version (~156 tok)
- `rsa.pyi` — This file is dual licensed under the terms of the Apache License, Version (~364 tok)
- `x25519.pyi` — This file is dual licensed under the terms of the Apache License, Version (~140 tok)
- `x448.pyi` — This file is dual licensed under the terms of the Apache License, Version (~135 tok)

## .build-venv/Lib/site-packages/cryptography/hazmat/bindings/openssl/

- `__init__.py` — This file is dual licensed under the terms of the Apache License, Version (~52 tok)
- `_conditional.py` — This file is dual licensed under the terms of the Apache License, Version (~1604 tok)
- `binding.py` — This file is dual licensed under the terms of the Apache License, Version (~1059 tok)

## .build-venv/Lib/site-packages/cryptography/hazmat/decrepit/

- `__init__.py` — This file is dual licensed under the terms of the Apache License, Version (~62 tok)

## .build-venv/Lib/site-packages/cryptography/hazmat/decrepit/ciphers/

- `__init__.py` — This file is dual licensed under the terms of the Apache License, Version (~62 tok)
- `algorithms.py` — This file is dual licensed under the terms of the Apache License, Version (~1019 tok)
- `modes.py` — This file is dual licensed under the terms of the Apache License, Version (~472 tok)

## .build-venv/Lib/site-packages/cryptography/hazmat/primitives/

- `__init__.py` — This file is dual licensed under the terms of the Apache License, Version (~52 tok)
- `_asymmetric.py` — This file is dual licensed under the terms of the Apache License, Version (~152 tok)
- `_cipheralgorithm.py` — This file is dual licensed under the terms of the Apache License, Version (~435 tok)
- `_modes.py` — This file is dual licensed under the terms of the Apache License, Version (~879 tok)
- `_serialization.py` — This file is dual licensed under the terms of the Apache License, Version (~1269 tok)
- `cmac.py` — This file is dual licensed under the terms of the Apache License, Version (~97 tok)
- `constant_time.py` — This file is dual licensed under the terms of the Apache License, Version (~121 tok)
- `hashes.py` — This file is dual licensed under the terms of the Apache License, Version (~1482 tok)
- `hmac.py` — This file is dual licensed under the terms of the Apache License, Version (~121 tok)
- `hpke.py` — This file is dual licensed under the terms of the Apache License, Version (~248 tok)
- `keywrap.py` — This file is dual licensed under the terms of the Apache License, Version (~1651 tok)
- `padding.py` — This file is dual licensed under the terms of the Apache License, Version (~533 tok)
- `poly1305.py` — This file is dual licensed under the terms of the Apache License, Version (~102 tok)

## .build-venv/Lib/site-packages/cryptography/hazmat/primitives/asymmetric/

- `__init__.py` — This file is dual licensed under the terms of the Apache License, Version (~52 tok)
- `dh.py` — This file is dual licensed under the terms of the Apache License, Version (~1118 tok)
- `dsa.py` — This file is dual licensed under the terms of the Apache License, Version (~1281 tok)
- `ec.py` — This file is dual licensed under the terms of the Apache License, Version (~2910 tok)
- `ed25519.py` — This file is dual licensed under the terms of the Apache License, Version (~857 tok)
- `ed448.py` — This file is dual licensed under the terms of the Apache License, Version (~1144 tok)
- `mldsa.py` — This file is dual licensed under the terms of the Apache License, Version (~3929 tok)
- `mlkem.py` — This file is dual licensed under the terms of the Apache License, Version (~2258 tok)
- `padding.py` — This file is dual licensed under the terms of the Apache License, Version (~816 tok)
- `rsa.py` — This file is dual licensed under the terms of the Apache License, Version (~2427 tok)
- `types.py` — This file is dual licensed under the terms of the Apache License, Version (~689 tok)
- `utils.py` — This file is dual licensed under the terms of the Apache License, Version (~235 tok)
- `x25519.py` — This file is dual licensed under the terms of the Apache License, Version (~1111 tok)
- `x448.py` — This file is dual licensed under the terms of the Apache License, Version (~1118 tok)

## .build-venv/Lib/site-packages/cryptography/hazmat/primitives/ciphers/

- `__init__.py` — This file is dual licensed under the terms of the Apache License, Version (~195 tok)
- `aead.py` — This file is dual licensed under the terms of the Apache License, Version (~182 tok)
- `algorithms.py` — This file is dual licensed under the terms of the Apache License, Version (~999 tok)
- `base.py` — This file is dual licensed under the terms of the Apache License, Version (~1216 tok)
- `modes.py` — This file is dual licensed under the terms of the Apache License, Version (~1714 tok)

## .build-venv/Lib/site-packages/cryptography/hazmat/primitives/kdf/

- `__init__.py` — This file is dual licensed under the terms of the Apache License, Version (~297 tok)
- `argon2.py` — This file is dual licensed under the terms of the Apache License, Version (~181 tok)
- `concatkdf.py` — This file is dual licensed under the terms of the Apache License, Version (~169 tok)
- `hkdf.py` — This file is dual licensed under the terms of the Apache License, Version (~156 tok)
- `kbkdf.py` — This file is dual licensed under the terms of the Apache License, Version (~211 tok)
- `pbkdf2.py` — This file is dual licensed under the terms of the Apache License, Version (~134 tok)
- `scrypt.py` — This file is dual licensed under the terms of the Apache License, Version (~169 tok)
- `x963kdf.py` — This file is dual licensed under the terms of the Apache License, Version (~131 tok)

## .build-venv/Lib/site-packages/cryptography/hazmat/primitives/serialization/

- `__init__.py` — This file is dual licensed under the terms of the Apache License, Version (~488 tok)
- `base.py` — This file is dual licensed under the terms of the Apache License, Version (~176 tok)
- `pkcs12.py` — This file is dual licensed under the terms of the Apache License, Version (~1459 tok)
- `pkcs7.py` — This file is dual licensed under the terms of the Apache License, Version (~4000 tok)
- `ssh.py` — This file is dual licensed under the terms of the Apache License, Version (~15369 tok)

## .build-venv/Lib/site-packages/cryptography/hazmat/primitives/twofactor/

- `__init__.py` — This file is dual licensed under the terms of the Apache License, Version (~74 tok)
- `hotp.py` — This file is dual licensed under the terms of the Apache License, Version (~931 tok)
- `totp.py` — This file is dual licensed under the terms of the Apache License, Version (~472 tok)

## .build-venv/Lib/site-packages/cryptography/x509/

- `__init__.py` — This file is dual licensed under the terms of the Apache License, Version (~2364 tok)
- `base.py` — This file is dual licensed under the terms of the Apache License, Version (~7731 tok)
- `certificate_transparency.py` — This file is dual licensed under the terms of the Apache License, Version (~228 tok)
- `extensions.py` — This file is dual licensed under the terms of the Apache License, Version (~22277 tok)
- `general_name.py` — This file is dual licensed under the terms of the Apache License, Version (~2239 tok)
- `name.py` — This file is dual licensed under the terms of the Apache License, Version (~4448 tok)
- `ocsp.py` — This file is dual licensed under the terms of the Apache License, Version (~3629 tok)
- `oid.py` — This file is dual licensed under the terms of the Apache License, Version (~266 tok)
- `verification.py` — This file is dual licensed under the terms of the Apache License, Version (~274 tok)

## .build-venv/Lib/site-packages/esp_rfc2217_server/

- `__init__.py` — SPDX-FileCopyrightText: 2009-2015 Chris Liechti (~1351 tok)
- `__main__.py` — SPDX-FileCopyrightText: 2014-2024 Fredrik Ahlberg, Angus Gratton, (~76 tok)
- `esp_port_manager.py` — SPDX-FileCopyrightText: 2014-2024 Fredrik Ahlberg, Angus Gratton, (~1031 tok)
- `redirector.py` — SPDX-FileCopyrightText: 2014-2024 Fredrik Ahlberg, Angus Gratton, (~883 tok)

## .build-venv/Lib/site-packages/espefuse/

- `__init__.py` — SPDX-FileCopyrightText: 2016-2025 Espressif Systems (Shanghai) CO LTD (~2054 tok)
- `__main__.py` — SPDX-FileCopyrightText: 2016-2022 Espressif Systems (Shanghai) CO LTD (~53 tok)
- `cli_util.py` — SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD (~2633 tok)
- `efuse_interface.py` — SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD (~2126 tok)

## .build-venv/Lib/site-packages/espefuse/efuse/

- `__init__.py` (~0 tok)
- `base_fields.py` — This file describes the common eFuses structures for chips (~14470 tok)
- `base_operations.py` — This file includes the common operations with eFuses for chips (~14874 tok)
- `csv_table_parser.py` — This file helps to parse CSV eFuse tables (~2784 tok)
- `emulate_efuse_controller_base.py` — This file describes eFuses controller for ESP32 chip (~2918 tok)
- `mem_definition_base.py` — This file describes eFuses fields and registers for ESP32 chip (~2874 tok)
- `util.py` — This file consists of the common useful functions for eFuse (~590 tok)

## .build-venv/Lib/site-packages/espefuse/efuse/esp32/

- `__init__.py` (~44 tok)
- `emulate_efuse_controller.py` — This file describes eFuses controller for ESP32 chip (~1572 tok)
- `fields.py` — This file describes eFuses for ESP32 chip (~4218 tok)
- `mem_definition.py` — This file describes eFuses fields and registers for ESP32 chip (~1814 tok)
- `operations.py` — This file includes the operations with eFuses for ESP32 chip (~3589 tok)

## .build-venv/Lib/site-packages/espefuse/efuse/esp32c2/

- `__init__.py` (~45 tok)
- `emulate_efuse_controller.py` — This file describes eFuses controller for ESP32-C2 chip (~1106 tok)
- `fields.py` — This file describes eFuses for ESP32-C2 chip (~3471 tok)
- `mem_definition.py` — This file describes eFuses fields and registers for ESP32-C2 chip (~1611 tok)
- `operations.py` — This file includes the operations with eFuses for ESP32-C2 chip (~3352 tok)

## .build-venv/Lib/site-packages/espefuse/efuse/esp32c3/

- `__init__.py` (~45 tok)
- `emulate_efuse_controller.py` — This file describes eFuses controller for ESP32-C3 chip (~965 tok)
- `fields.py` — This file describes eFuses for ESP32-C3 chip (~3884 tok)
- `mem_definition.py` — This file describes eFuses fields and registers for ESP32-C3 chip (~2176 tok)
- `operations.py` — This file includes the operations with eFuses for ESP32-C3 chip (~3699 tok)

## .build-venv/Lib/site-packages/espefuse/efuse/esp32c5/

- `__init__.py` (~45 tok)
- `emulate_efuse_controller.py` — This file describes eFuses controller for ESP32-C5 chip (~964 tok)
- `fields.py` — This file describes eFuses for ESP32-C5 chip (~4519 tok)
- `mem_definition.py` — This file describes eFuses fields and registers for ESP32-C5 chip (~2168 tok)
- `operations.py` — This file includes the operations with eFuses for ESP32-C5 chip (~3436 tok)

## .build-venv/Lib/site-packages/espefuse/efuse/esp32c6/

- `__init__.py` (~45 tok)
- `emulate_efuse_controller.py` — This file describes eFuses controller for ESP32-C6 chip (~964 tok)
- `fields.py` — This file describes eFuses for ESP32-C6 chip (~3890 tok)
- `mem_definition.py` — This file describes eFuses fields and registers for ESP32-C6 chip (~1925 tok)
- `operations.py` — This file includes the operations with eFuses for ESP32-C6 chip (~3866 tok)

## .build-venv/Lib/site-packages/espefuse/efuse/esp32c61/

- `__init__.py` (~45 tok)
- `emulate_efuse_controller.py` — This file describes eFuses controller for ESP32-C61 chip (~964 tok)
- `fields.py` — This file describes eFuses for ESP32-C61 chip (~3587 tok)
- `mem_definition.py` — This file describes eFuses fields and registers for ESP32-C61 chip (~1820 tok)
- `operations.py` — This file includes the operations with eFuses for ESP32-C61 chip (~3453 tok)

## .build-venv/Lib/site-packages/espefuse/efuse/esp32e22/

- `__init__.py` (~45 tok)
- `emulate_efuse_controller.py` — This file describes eFuses controller for ESP32-E22 chip (~966 tok)
- `fields.py` — This file describes eFuses for ESP32E22 chip (~3765 tok)
- `mem_definition.py` — This file describes eFuses fields and registers for ESP32-E22 chip (~1821 tok)
- `operations.py` — This file includes the operations with eFuses for ESP32-E22 chip (~3298 tok)

## .build-venv/Lib/site-packages/espefuse/efuse/esp32h2/

- `__init__.py` (~45 tok)
- `emulate_efuse_controller.py` — This file describes eFuses controller for ESP32-H2 chip (~957 tok)
- `fields.py` — This file describes eFuses for ESP32-H2 chip (~3873 tok)
- `mem_definition.py` — This file describes eFuses fields and registers for ESP32-H2 chip (~1948 tok)
- `operations.py` — This file includes the operations with eFuses for ESP32-H2 chip (~4026 tok)

## .build-venv/Lib/site-packages/espefuse/efuse/esp32h21/

- `__init__.py` (~45 tok)
- `emulate_efuse_controller.py` — This file describes eFuses controller for ESP32-H21 chip (~958 tok)
- `fields.py` — This file describes eFuses for ESP32-H21 chip (~3606 tok)
- `mem_definition.py` — This file describes eFuses fields and registers for ESP32-H21 chip (~1926 tok)
- `operations.py` — This file includes the operations with eFuses for ESP32-H21 chip (~3378 tok)

## .build-venv/Lib/site-packages/espefuse/efuse/esp32h4/

- `__init__.py` (~45 tok)
- `emulate_efuse_controller.py` — This file describes eFuses controller for ESP32-H4 chip (~957 tok)
- `fields.py` — This file describes eFuses for ESP32-H4 chip (~4199 tok)
- `mem_definition.py` — This file describes eFuses fields and registers for ESP32-H4 chip (~1946 tok)
- `operations.py` — This file includes the operations with eFuses for ESP32-H4 chip (~3396 tok)

## .build-venv/Lib/site-packages/espefuse/efuse/esp32p4/

- `__init__.py` (~45 tok)
- `emulate_efuse_controller.py` — This file describes eFuses controller for ESP32-P4 chip (~964 tok)
- `fields.py` — This file describes eFuses for ESP32-P4 chip (~5214 tok)
- `mem_definition.py` — This file describes eFuses fields and registers for ESP32-P4 chip (~2144 tok)
- `operations.py` — This file includes the operations with eFuses for ESP32-P4 chip (~3473 tok)

## .build-venv/Lib/site-packages/espefuse/efuse/esp32s2/

- `__init__.py` (~45 tok)
- `emulate_efuse_controller.py` — This file describes eFuses controller for ESP32-S2 chip (~969 tok)
- `fields.py` — This file describes eFuses for ESP32S2 chip (~4394 tok)
- `mem_definition.py` — This file describes eFuses fields and registers for ESP32 chip (~2344 tok)
- `operations.py` — This file includes the operations with eFuses for ESP32S2 chip (~4618 tok)

## .build-venv/Lib/site-packages/espefuse/efuse/esp32s3/

- `__init__.py` (~45 tok)
- `emulate_efuse_controller.py` — This file describes eFuses controller for ESP32-S3 chip (~959 tok)
- `fields.py` — This file describes eFuses for ESP32-S3 chip (~4557 tok)
- `mem_definition.py` — This file describes eFuses fields and registers for ESP32-S3 chip (~2043 tok)
- `operations.py` — This file includes the operations with eFuses for ESP32-S3 chip (~4658 tok)

## .build-venv/Lib/site-packages/espefuse/efuse/esp32s31/

- `__init__.py` (~45 tok)
- `emulate_efuse_controller.py` — This file describes eFuses controller for ESP32-S31 chip (~937 tok)
- `fields.py` — This file describes eFuses for ESP32-S31 chip (~4024 tok)
- `mem_definition.py` — This file describes eFuses fields and registers for ESP32-S31 chip (~1831 tok)
- `operations.py` — This file includes the operations with eFuses for ESP32-S31 chip (~3397 tok)

## .build-venv/Lib/site-packages/espefuse/efuse_defs/

- `esp32.yaml` (~5243 tok)
- `esp32c2.yaml` (~4685 tok)
- `esp32c3.yaml` — Declares of (~9896 tok)
- `esp32c5.yaml` — Declares in (~12371 tok)
- `esp32c6.yaml` — Declares of (~10095 tok)
- `esp32c61.yaml` — Declares of (~9867 tok)
- `esp32e22.yaml` — Declares that (~6925 tok)
- `esp32h2_v0.0_v1.1.yaml` (~120 tok)
- `esp32h2.yaml` — Declares of (~9874 tok)
- `esp32h21.yaml` — Declares in (~9302 tok)
- `esp32h4.yaml` — Declares of (~12936 tok)
- `esp32p4_v3.0.yaml` — Declares of (~16605 tok)
- `esp32p4.yaml` — Declares of (~13805 tok)
- `esp32s2.yaml` — Declares that (~10235 tok)
- `esp32s3.yaml` — Declares that (~12122 tok)
- `esp32s31.yaml` — Declares of (~13498 tok)

## .build-venv/Lib/site-packages/espsecure/

- `__init__.py` — SPDX-FileCopyrightText: 2016-2025 Espressif Systems (Shanghai) CO LTD (~19940 tok)
- `__main__.py` — SPDX-FileCopyrightText: 2016-2022 Espressif Systems (Shanghai) CO LTD (~54 tok)

## .build-venv/Lib/site-packages/espsecure/esp_hsm_sign/

- `__init__.py` — SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD (~1943 tok)
- `exceptions.py` — SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD (~595 tok)

## .build-venv/Lib/site-packages/esptool-5.3.1.dist-info/

- `entry_points.txt` (~87 tok)
- `INSTALLER` (~2 tok)
- `METADATA` (~1139 tok)
- `RECORD` (~6278 tok)
- `REQUESTED` (~0 tok)
- `top_level.txt` (~12 tok)
- `WHEEL` (~25 tok)

## .build-venv/Lib/site-packages/esptool-5.3.1.dist-info/licenses/

- `LICENSE` — Project license (~4825 tok)

## .build-venv/Lib/site-packages/esptool/

- `__init__.py` — SPDX-FileCopyrightText: 2014-2025 Fredrik Ahlberg, Angus Gratton, (~12259 tok)
- `__main__.py` — SPDX-FileCopyrightText: 2014-2022 Fredrik Ahlberg, Angus Gratton, (~71 tok)
- `bin_image.py` — SPDX-FileCopyrightText: 2014-2026 Fredrik Ahlberg, Angus Gratton, (~16476 tok)
- `cli_util.py` — SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD (~6968 tok)
- `cmds.py` — SPDX-FileCopyrightText: 2014-2025 Fredrik Ahlberg, Angus Gratton, (~37496 tok)
- `config.py` — SPDX-FileCopyrightText: 2014-2025 Espressif Systems (Shanghai) CO LTD, (~915 tok)
- `loader.py` — SPDX-FileCopyrightText: 2014-2025 Fredrik Ahlberg, Angus Gratton, (~25107 tok)
- `logger.py` — SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD (~2494 tok)
- `reset.py` — SPDX-FileCopyrightText: 2014-2025 Fredrik Ahlberg, Angus Gratton, (~2523 tok)
- `uf2_writer.py` — SPDX-FileCopyrightText: 2020-2025 Espressif Systems (Shanghai) CO LTD (~845 tok)
- `util.py` — SPDX-FileCopyrightText: 2014-2025 Fredrik Ahlberg, Angus Gratton, (~2502 tok)

## .build-venv/Lib/site-packages/esptool/targets/

- `__init__.py` (~281 tok)
- `esp32.py` — SPDX-FileCopyrightText: 2014-2025 Fredrik Ahlberg, Angus Gratton, (~4666 tok)
- `esp32c2.py` — SPDX-FileCopyrightText: 2014-2025 Fredrik Ahlberg, Angus Gratton, (~1829 tok)
- `esp32c3.py` — SPDX-FileCopyrightText: 2014-2025 Fredrik Ahlberg, Angus Gratton, (~2696 tok)
- `esp32c5.py` — SPDX-FileCopyrightText: 2024-2025 Fredrik Ahlberg, Angus Gratton, (~2320 tok)
- `esp32c6.py` — SPDX-FileCopyrightText: 2024-2025 Fredrik Ahlberg, Angus Gratton, (~2182 tok)
- `esp32c61.py` — SPDX-FileCopyrightText: 2024-2025 Fredrik Ahlberg, Angus Gratton, (~1096 tok)
- `esp32e22.py` — SPDX-FileCopyrightText: 2026 Fredrik Ahlberg, Angus Gratton, (~2396 tok)
- `esp32h2.py` — SPDX-FileCopyrightText: 2024-2025 Fredrik Ahlberg, Angus Gratton, (~908 tok)
- `esp32h21.py` — SPDX-FileCopyrightText: 2024-2025 Fredrik Ahlberg, Angus Gratton, (~1083 tok)
- `esp32h4.py` — SPDX-FileCopyrightText: 2025 Fredrik Ahlberg, Angus Gratton, (~2445 tok)
- `esp32p4.py` — SPDX-FileCopyrightText: 2024-2025 Fredrik Ahlberg, Angus Gratton, (~4033 tok)
- `esp32s2.py` — SPDX-FileCopyrightText: 2014-2025 Fredrik Ahlberg, Angus Gratton, (~3180 tok)
- `esp32s3.py` — SPDX-FileCopyrightText: 2014-2025 Fredrik Ahlberg, Angus Gratton, (~4040 tok)
- `esp32s31.py` — SPDX-FileCopyrightText: 2025-2026 Fredrik Ahlberg, Angus Gratton, (~2416 tok)
- `esp8266.py` — SPDX-FileCopyrightText: 2014-2025 Fredrik Ahlberg, Angus Gratton, (~1563 tok)

## .build-venv/Lib/site-packages/esptool/targets/stub_flasher/1/

- `esp32.json` (~1451 tok)
- `esp32c2.json` (~1384 tok)
- `esp32c3.json` (~1557 tok)
- `esp32c5.json` (~1981 tok)
- `esp32c6.json` (~1525 tok)
- `esp32h2.json` (~1525 tok)
- `esp32p4-rev1.json` (~2162 tok)
- `esp32s2.json` (~1810 tok)
- `esp32s3.json` (~2301 tok)
- `esp8266.json` (~3488 tok)
- `README.md` — Project documentation (~74 tok)

## .build-venv/Lib/site-packages/esptool/targets/stub_flasher/2/

- `esp32.json` (~2463 tok)
- `esp32c2.json` (~2415 tok)
- `esp32c3.json` (~2415 tok)
- `esp32c5.json` (~3091 tok)
- `esp32c6.json` (~2812 tok)
- `esp32c61.json` (~2525 tok)
- `esp32h2.json` (~2411 tok)
- `esp32h4.json` (~2419 tok)
- `esp32p4-rev1.json` (~2904 tok)
- `esp32p4.json` (~2904 tok)
- `esp32s2.json` (~2686 tok)
- `esp32s3.json` (~4723 tok)
- `esp32s31.json` (~2559 tok)
- `esp8266.json` (~5632 tok)
- `LICENSE-APACHE` — Declares name (~3031 tok)
- `LICENSE-MIT` (~292 tok)
- `README.md` — Project documentation (~69 tok)

## .build-venv/Lib/site-packages/intelhex-2.3.0.dist-info/

- `AUTHORS.rst` (~143 tok)
- `INSTALLER` (~2 tok)
- `LICENSE.txt` (~382 tok)
- `METADATA` (~713 tok)
- `RECORD` (~607 tok)
- `top_level.txt` (~3 tok)
- `WHEEL` (~30 tok)

## .build-venv/Lib/site-packages/intelhex/

- `__init__.py` — Intel HEX format manipulation library. (~14887 tok)
- `__main__.py` — All rights reserved. (~536 tok)
- `__version__.py` — IntelHex library version information (~36 tok)
- `bench.py` — (c) Alexander Belchenko, 2007, 2009 (~2655 tok)
- `compat.py` — Compatibility functions for python 2 and 3. (~1439 tok)
- `getsizeof.py` — Recursive version sys.getsizeof(). Extendable with custom handlers. (~622 tok)
- `test.py` — Test suite for IntelHex library. (~20185 tok)

## .build-venv/Lib/site-packages/markdown_it/

- `__init__.py` — A Python port of Markdown-It (~33 tok)
- `_compat.py` (~10 tok)
- `_punycode.py` — Permission is hereby granted, free of charge, to any person obtaining (~678 tok)
- `main.py` — MarkdownIt: set, configure, get_all_rules, get_active_rules + 11 more (~3646 tok)
- `parser_block.py` — Block-level tokenizer. (~1126 tok)
- `parser_core.py` — Core: process (~291 tok)
- `parser_inline.py` — Tokenizes paragraph content. (~2081 tok)
- `port.yaml` — Declares property (~700 tok)
- `py.typed` — Marker file for PEP 561 (~7 tok)
- `renderer.py` — Renderer: render, token_type_name, strong_open, strong_close + 15 more (~3037 tok)
- `ruler.py` — Ruler: src, src, srcCharCode, at + 9 more (~2612 tok)
- `token.py` — Token: convert_attrs, attrIndex, attrItems, attrPush + 6 more (~1824 tok)
- `tree.py` — A tree representation of a linear markdown-it token stream. (~3175 tok)
- `utils.py` — OptionsType: maxNesting, maxNesting, html, html + 15 more (~1755 tok)

## .build-venv/Lib/site-packages/markdown_it/cli/

- `__init__.py` (~0 tok)
- `parse.py` — main, convert, convert_stdin, convert_file + 3 more (~961 tok)

## .build-venv/Lib/site-packages/markdown_it/common/

- `__init__.py` (~0 tok)
- `entities.py` — HTML5 entities map: { name -> characters }. (~45 tok)
- `html_blocks.py` — List of valid html blocks names, according to commonmark spec (~282 tok)
- `html_re.py` — Regexps to match html elements (~265 tok)
- `normalize_url.py` — normalizeLink, normalizeLinkText, validateLink (~734 tok)
- `utils.py` — Utilities for parsing source text (~2488 tok)

## .build-venv/Lib/site-packages/markdown_it/helpers/

- `__init__.py` — Functions for parsing Links (~73 tok)
- `parse_link_destination.py` — _Result: parseLinkDestination (~545 tok)
- `parse_link_label.py` — parseLinkLabel (~297 tok)
- `parse_link_title.py` — Parse link title (~650 tok)

## .build-venv/Lib/site-packages/markdown_it/presets/

- `__init__.py` — gfm_like: make, make (~453 tok)
- `commonmark.py` — Commonmark default options. (~812 tok)
- `default.py` — markdown-it default options. (~510 tok)
- `zero.py` — make (~596 tok)

## .build-venv/Lib/site-packages/markdown_it/rules_block/

- `__init__.py` (~170 tok)
- `blockquote.py` — Block quotes (~3288 tok)
- `code.py` — Code block (4 spaces padded). (~246 tok)
- `fence.py` — fences (``` lang, ~~~ lang) (~1367 tok)
- `heading.py` — Atex heading (#, ##, ...) (~499 tok)
- `hr.py` — Horizontal rule (~351 tok)
- `html_block.py` — HTML block (~778 tok)
- `lheading.py` — lheading (---, ==) (~750 tok)
- `list.py` — to: skipBulletListMarker, skipOrderedListMarker, markTightParagraphs, list_block (~3440 tok)
- `paragraph.py` — Paragraph. (~520 tok)
- `reference.py` — reference, getNextLine (~1996 tok)
- `state_block.py` — StateBlock: push, isEmpty, skipEmptyLines, skipSpaces + 7 more (~2407 tok)
- `table.py` — GFM table, https://github.github.com/gfm/#tables-extension- (~2195 tok)

## .build-venv/Lib/site-packages/markdown_it/rules_core/

- `__init__.py` (~113 tok)
- `block.py` — block (~107 tok)
- `inline.py` — inline (~93 tok)
- `linkify.py` — _LinkType: linkify (~1469 tok)
- `normalize.py` — Normalize input string. (~116 tok)
- `replacements.py` — Simple typographic replacements (~979 tok)
- `smartquotes.py` — Convert straight quotation marks to typographic ones (~2126 tok)
- `state_core.py` — Declares StateCore (~163 tok)
- `text_join.py` — Join raw text tokens with the rest of the text (~571 tok)

## .build-venv/Lib/site-packages/markdown_it/rules_inline/

- `__init__.py` (~199 tok)
- `autolink.py` — Process autolinks '<protocol:...>' (~590 tok)
- `backticks.py` — Parse backticks (~582 tok)
- `balance_pairs.py` — Balance paired characters (*, _, etc) in inline tokens. (~1387 tok)
- `emphasis.py` — Process *this* and _that_ (~893 tok)
- `entity.py` — Process html entity - &#123;, &#xAF;, &quot;, ... (~472 tok)
- `escape.py` — escape (~474 tok)
- `fragments_join.py` — fragments_join (~567 tok)
- `html_inline.py` — Process html tags (~323 tok)
- `image.py` — Process ![image](<src> "title") (~1184 tok)
- `link.py` — Process [link](<to> "stuff") (~1217 tok)
- `linkify.py` — Process links like https://example.org/ (~488 tok)
- `newline.py` — Proceess '\n'. (~371 tok)
- `state_inline.py` — from: pushPending, push, scanDelims (~1427 tok)
- `strikethrough.py` — ~~strike through~~ (and optionally ~single tilde~) (~1477 tok)
- `text.py` — Skip text characters for text token, place those to pending buffer (~160 tok)

## .build-venv/Lib/site-packages/markdown_it_py-4.2.0.dist-info/

- `entry_points.txt` (~15 tok)
- `INSTALLER` (~2 tok)
- `METADATA` (~1976 tok)
- `RECORD` (~2895 tok)
- `WHEEL` (~22 tok)

## .build-venv/Lib/site-packages/markdown_it_py-4.2.0.dist-info/licenses/

- `LICENSE` — Project license (~288 tok)
- `LICENSE.markdown-it` (~287 tok)

## .build-venv/Lib/site-packages/mdurl/

- `__init__.py` (~157 tok)
- `_decode.py` — get_decode_cache, decode, repl_func_with_cache (~859 tok)
- `_encode.py` — get_encode_cache, encode (~744 tok)
- `_format.py` — format (~179 tok)

## src/OTGW-firmware/data/

- `v2.css` — Styles: 13 vars (~15274 tok)
- `v2.html` — OTGW firmware (~11760 tok)
- `v2.js` — applyTheme: initTheme, showPage, showDesign + 8 more (~62571 tok)
