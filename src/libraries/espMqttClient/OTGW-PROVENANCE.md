# espMqttClient provenance (ADR-264)

`src/` in this directory is a byte copy of a pinned tag of our fork. It is never edited in place: a change goes to the fork first,
gets a new tag, and is then synced here with this file updated in the same commit. `tests/test_espmqttclient_provenance.py` recomputes the
hash below (CRLF read as LF) and fails when the copy and this file disagree.

- fork: https://github.com/rvdbreemen/espMqttClient
- tag: v1.7.3-otgw.2
- tag commit: 0fe8227bcf3f42f666c30a00ec2e4f607d6507a2
- upstream base: de31cb2
- sha256 of src/: 9af6447c97583b21609e802fa5b37eb68ca9f6d9d2ee6cdff265f0df43708aa0

What the fork adds on top of upstream de31cb2 (one commit each): the PR #191 non-blocking ClientSync::write(), the TASK-1325
EMC_CONNECT_TIMEOUT_MS option, and the ADR-266 EMC_TCP_NODELAY option (default 1 = upstream behaviour; the firmware builds with 0 in platformio.ini).
