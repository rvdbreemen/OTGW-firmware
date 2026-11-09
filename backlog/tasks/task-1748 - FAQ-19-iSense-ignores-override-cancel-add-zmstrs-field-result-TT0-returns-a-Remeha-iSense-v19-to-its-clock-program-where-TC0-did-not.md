---
id: TASK-1748
title: >-
  FAQ 19 (iSense ignores override cancel): add zmstr's field result, TT=0
  returns a Remeha iSense v19 to its clock program where TC=0 did not
status: To Do
assignee: []
created_date: '2026-11-09T08:47:00Z'
labels:
  - docs
  - faq
  - needs-maintainer-ok
dependencies: []
ordinal: 848000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
zmstr, #nederlandse-ondersteuning 2026-11-09 08:37 UTC (msg 1569269775258099766, https://discord.com/channels/812969634638725140/815561033036333076/1569269775258099766): 'In mijn situatie werkt TT=0 wel goed ipv TC=0. Dan springt hij netjes terug naar het klok programma. Je moet het inderdaad wel even wat tijd geven voordat het aankomt en verwerkt is'. Thread: his question 2026-11-02 19:16 (msg 1566893868665012285), bot answers 1566897047007666230 + 1566899229450960988 (TC=0, wait, SR=99:0,1 + CR=99), FAQ 19 https://github.com/rvdbreemen/OTGW-firmware/wiki/NL-Veelgestelde-vragen#19-mijn-thermostaat-negeert-de-kamertemperatuur-die-ik-stuur . Single field report (one iSense v19); why TT=0 works where TC=0 did not is not verified (likely depends on how the override was set). Robert decides whether FAQ 19 gets this as a tip.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 FAQ 19 (NL and EN) mentions TT=0 as a tip from a user's field report, with the 'give it a few minutes' note, or Robert decides not to
<!-- AC:END -->
