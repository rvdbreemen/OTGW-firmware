---
id: TASK-2015
title: >-
  SAT web UI: the weather hint asks for latitude/longitude while HA pushes
  sat/sun_elevation (thaghostnl)
status: To Do
assignee: []
created_date: '2026-11-18T06:34:00Z'
updated_date: '2026-11-18T06:38:00Z'
labels:
  - webui
  - sat
  - needs-maintainer-ok
dependencies: []
priority: low
ordinal: 1033000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Reported by thaghostnl in #alpha-testing 2026-11-17 20:35 UTC (https://discord.com/channels/812969634638725140/1514720723980259460/1572349530458816624), beta.170, solar gain on, weather fetch off, outdoor temperature via sat/outdoor_temp: the SAT page says 'Location detection needs HTTPS. Enter Latitude and Longitude...', although a fresh external sat/sun_elevation (younger than 1 h) takes precedence (TASK-1410). Robert (maintainer, relayed by the coordinator 2026-11-18): "Niet iedereen gebruikt Home Assistant (HA). Dus dat is geen given, maar als HA de zonnestand doorgeeft, dan mag je dat gebruiken natuurlijk." So latitude/longitude stays the default input for users without HA; when HA pushes sat/sun_elevation, the card uses that and does not ask for a location.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 With a fresh external sat/sun_elevation the weather card names that source and asks for no location (screenshot)
- [ ] #2 The latitude/longitude and solar-gain help name sat/sun_elevation as the alternative
- [ ] #3 Both paths shown with screenshots: without HA the card asks for latitude/longitude; with a fresh sat/sun_elevation from HA it names that source and asks for no location
<!-- AC:END -->
