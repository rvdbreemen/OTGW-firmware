---
id: TASK-1131
title: >-
  ESP-IDF log output leaks onto the PIC serial line and the PIC answers it as
  menu input
status: To Do
assignee: []
created_date: '2026-09-05 20:39'
labels:
  - bug
  - pic
  - serial
dependencies: []
priority: high
ordinal: 279000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Made visible by the new diagnose screen (TASK-1128) on the bench ESP32-S3 at alpha.362/363, esp32-classic, against a pic16f1847 running diagnostic firmware 2.2. The diagnose console repeatedly shows lines like: 'Enter test number: E (89325) task_wdt: esp_task_wdt_reset(707): task not found' immediately followed by 'Invalid test' and a menu redraw. Read that sequence carefully: the PIC is ECHOING an ESP-IDF error log back at us, which means the ESP wrote that log text INTO the PIC UART. The PIC then treats it as a menu choice, rejects it, and redraws. On the classic-on-S3 pin map the PIC UART is UART0 (GPIO43/44), which is also where the IDF console logs by default, so this is the known UART0/console overlap. Two defects are stacked here: the console leak itself, and whatever calls esp_task_wdt_reset() from a task that is not subscribed to the task watchdog. The leak is the more serious of the two, because the ESP is injecting bytes into the PIC on any board whose PIC UART shares UART0, which on a gateway PIC would mean unsolicited command text rather than a harmless rejected menu choice.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 No ESP-IDF log output can reach the PIC UART on any board where the PIC shares UART0
- [ ] #2 The esp_task_wdt_reset call from an unsubscribed task is found and either subscribed or removed
- [ ] #3 The diagnose console shows only PIC output, with no interleaved firmware log lines
<!-- AC:END -->
