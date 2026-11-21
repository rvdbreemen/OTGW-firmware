# Headless Chromium + Node 22 for web UI checks against bench units, run on the NAS (Robert 2026-11-21:
# all testing on the NAS). Versions pinned (TASK-2990): alpine 3.21 by digest, packages by exact apk version.
# Build on the NAS: cd /volume1/docker/otgw-bench/chromium && docker build -t otgw-bench-chromium:alpine3.21 .
# Run a check: scripts/nas-bench/uicheck.sh. Chromium flags for the NAS kernel are in ui/task2982.mjs.
FROM alpine:3.21@sha256:bc21487101ae47172509130ee1092cc273a831968d64b617776d1f69c81e42ab
RUN apk add --no-cache chromium=136.0.7103.113-r0 nodejs=22.23.5-r0 font-noto=24.7.1-r0 font-dejavu=2.37-r5 \
 && adduser -D -u 1000 bench
USER bench
WORKDIR /work
ENV CHROME=/usr/bin/chromium-browser
