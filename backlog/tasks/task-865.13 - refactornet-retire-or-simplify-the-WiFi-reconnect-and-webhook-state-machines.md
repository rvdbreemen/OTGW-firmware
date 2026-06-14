---
id: TASK-865.13
title: >-
  refactor(net): retire or simplify the WiFi-reconnect and webhook state
  machines
status: In Review
assignee:
  - '@claude'
created_date: '2026-06-13 05:57'
updated_date: '2026-06-14 10:47'
labels:
  - async-esp32s3
dependencies:
  - TASK-865.2
  - TASK-865.6
  - TASK-865.9
parent_task_id: TASK-865
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
## Why (ADR-123 Phase 4; depends seq2=TASK-865.2, seq6=TASK-865.6, seq9=TASK-865.9)
Retire/simplify the manual non-blocking state machines that exist only because the cooperative loop forbade blocking (ADR-047 WiFi reconnect, ADR-048 webhook, ADR-058 PIC PR=). Once a feature runs in its own task / async callback it MAY block, so the hand-rolled FSM can collapse. They are NOT uniformly redundant - investigate before deleting.

## Findings
- ADR-058 (PIC PR=) is ALREADY the correct fire-and-forget queue pattern (queryNextPICsetting/queryOTGWgatewaymode/getpicfwversion OTGW-Core.ino ~533/545/621 -> addCommandToQueue(...,true); async handlePRresponse() 4505). No busy-wait. Under the seq6 PIC task this queue IS the task ingress. ACTION: keep the code; record in the superseding ADR that "retire ADR-058" is satisfied (queue pattern survives unchanged). No code change.
- ADR-047 (WiFi) carries real product behaviour beyond blocking-avoidance - SIMPLIFY, not delete. loopWifi() (networkStuff.ino:302, enum WifiState_t 288): bounded retry-then-doRestart() (WIFI_FAILED 386-388), BETA captive-AP fallback (WIFI_AP_FALLBACK/RETRY 390-414, gated on _VERSION_PRERELEASE), Ethernet-failover preemption (early return when state.net.eMode==NET_ETHERNET, 303-312), and the issue #525 / TASK-432 DHCP-ownership rule (never DHCP-start while associated; do NOT re-add platformRestartDHCP()). If WiFi moves to a task it MAY block on association, collapsing CONNECTING states to linear connect-with-timeout, but retry-cap, AP fallback, Ethernet preemption and DHCP rules MUST be preserved.
- ADR-048 (webhook) still blocks synchronously - cleanest retirement. evalWebhook() (webhook.ino:290, enum WebhookState_t 285) detects a status-bit change then attemptSendWebhook() does a synchronous HTTPClient GET/POST 500ms timeout (225, called 319). The WH_* machine spreads that across loop turns. Change-detection (evalTriggerBit() 267) + retry-with-backoff are intrinsic and must survive; loop-spreading is incidental. In a webhook task / async client it collapses to: detect change -> block on send -> retry loop.

## What
1. WiFi: decide home (own task vs stay in loop). If task: rewrite loopWifi() linear connect-retry that may block, preserving retry-cap+reboot, BETA AP fallback, NET_ETHERNET preemption handshake, issue-#525 DHCP rules. If loop: leave + record retention. Supersede/amend ADR-047 (already Superseded-by ADR-075 for timeout params).
2. Webhook: move the synchronous send off the loop (own task or async client). Collapse WH_* to detect+send+retry, preserving evalTriggerBit() change detection, latest-state-supersedes, 3-attempt/30s backoff, isLocalUrl() (ADR-003/032) enforcement, ADR-004 (no String) error path. Supersede ADR-048; re-validate the ADR-057 webhook delivery contract.
3. PIC PR= (ADR-058): no code change; record the queue-pattern-is-PIC-task-ingress note in the superseding ADR.

## Acceptance Criteria
- build: esp32 + esp32-classic exit 0 after the refactor.
- evaluator: evaluate.py --quick no new failures; webhook error path stays ADR-004 compliant (no String/http.errorToString); abstraction unbroken. WiFi refactor preserves all four invariants - bounded-retry-then-reboot, BETA AP fallback (_VERSION_PRERELEASE), NET_ETHERNET preemption early-return, and NO platformRestartDHCP() (grep confirms absent).
- build: webhook still enforces isLocalUrl() before every send + detects bit changes via evalTriggerBit(); 3-attempt/30s backoff preserved; ADR-058 PR= queue path unchanged (no diff in queryNextPICsetting/handlePRresponse).
- Three 2.0.0-numbered ADRs (or one consolidated Phase-4 ADR) supersede/amend ADR-047/048/058, each cross-ref ADR-123, stating what survives vs collapses; ADR-057 re-validated.
- field: force a WiFi outage (power-cycle AP) -> reconnect, retry-cap-reboot, BETA AP captive fallback all work; Ethernet cable insert/remove preempts WiFi on the esp32 (W5500) board.
- field: trigger a webhook bit change to a local Shelly/HA endpoint -> GET + POST ({variable} payload) fire, retry on a down target, and never block the PIC UART / web UI under the new model.
<!-- SECTION:DESCRIPTION:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
ADR-123 Phase-4 landed: webhook synchronous HTTPClient send moved off the cooperative loop onto a dedicated FreeRTOS 'webhook' task (8KB stack). evalWebhook() keeps loop-side evalTriggerBit() edge detection + latest-state-supersedes and enqueues a self-contained value-copy WebhookJob; lock-free sender runs the 3-attempt/30s backoff as real platformTaskDelay, expands {vars} at enqueue time (brief OTStateLock for the AsyncTCP test endpoint) so OTStateLock is never held across the HTTP round-trip. testWebhook() now enqueues instead of blocking AsyncTCP. webhookInFlight preserves ADR-057 one-pending-at-a-time; isLocalUrl per send + ADR-004 (no String) error path preserved. WiFi (ADR-047): no code change, stays in loop (WiFi.begin async); all four invariants intact (bounded-retry-then-reboot, BETA AP fallback, NET_ETHERNET preempt, no platformRestartDHCP). PIC PR= (ADR-058): no code change, queue is now the PIC task TX ingress. ADR-136 supersedes ADR-048, amends ADR-047, records ADR-058 satisfied; ADR-057 re-validated. platform_esp32.h: added PLATFORM_QUEUE_WAIT_FOREVER sentinel for indefinite blocking receive. Remaining field-validation ACs: (1) force WiFi outage -> reconnect/retry-cap-reboot/BETA captive AP fallback, plus Ethernet insert/remove preempts WiFi on W5500; (2) trigger webhook bit change to a local Shelly/HA endpoint -> GET+POST fire, retry on down target, never block PIC UART/web UI.
<!-- SECTION:NOTES:END -->
