# TASK-3124 evidence (headless Chrome, real data/v2.js, 2026-11-27)

`capture.mjs` loads the real v2.js against the page-harness fake device and logs every WebSocket construction (seconds since load).

| scenario | code | attempts (s since load) | gaps (s) |
|---|---|---|---|
| refusing endpoint, 45 s | before (origin/dev) | 0.3 5.3 10.3 15.3 20.4 25.4 30.4 35.4 40.4 | 5 5 5 5 5 5 5 5 (fixed, forever) |
| refusing endpoint, 95 s | after | 0.4 4.6 13.1 33.8 78.2 | 4.2 8.6 20.7 44.3 (5 10 20 40 then 60, +-20 % jitter) |
| accept-then-drop endpoint | after | 0.3 5.3 14.9, then no further attempt | page pauses after the 3rd opened-and-closed socket |

Screenshots of the paused state on Monitor > Connection (row "Live link", Reconnect button): `paused-en.png`, `paused-nl.png`.
Raw lines: `before_refuse.json`, `after_refuse.json`, `after_flap_en.txt`, `after_flap_nl.txt`.
Not covered: a real device (bench .63/.76 browser check at B2/B3).
