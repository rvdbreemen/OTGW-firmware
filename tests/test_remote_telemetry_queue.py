#!/usr/bin/env python3
"""TASK-2901 poc.22: a transient upload failure or a skipped attempt costs no data.

09's poc.21 run on .63 (2026-11-19 21:08): the low-heap guard (16 KB free / 8 KB block) skipped an upload under replay
load; the skip counted as an upload failure, the upload backed off for 5 and then 11 minutes, and capture paused, so
the replayed frames of that time were never recorded. An upload had succeeded 11 s earlier at 14.4 KB free / 7.7 KB
block. poc.22: the guard is 12 KB / 4 KB, a skip is retried in 30 s without a backoff step, a 128-record PSRAM queue
holds the records meanwhile, and capture goes on before the first login and while the upload backs off.

Structural check on RemoteTelemetryDevice.h (the behaviour is proven on the bench, AC #6). Red on 047d0803d (poc.21).
poc.23: the task stack and the slot table in PSRAM, and no flash write from the task; red on 0c588f91e (poc.22).
poc.24: 'blocked' never turns the opt-in off; a full queue pauses capture (EV_PAUSE); red on c2623c057.
poc.27: a pause keeps the queue, 'blocked' and a uid clash keep capturing until the queue is full; red on b832b77df.
Run: python tests/test_remote_telemetry_queue.py
"""
import re
import unittest
from pathlib import Path

SRC = (Path(__file__).resolve().parent.parent / "src" / "OTGW-firmware" / "RemoteTelemetryDevice.h").read_text(
    encoding="utf-8").replace(chr(13) + chr(10), chr(10))


def body(signature):
    a = SRC.index(signature)
    return SRC[a:SRC.index("\n}\n", a)]


def const(name):
    m = re.search(r"static const \w+ %s\s*=\s*(\d+)\s*;" % name, SRC)
    return int(m.group(1)) if m else None


class Queue(unittest.TestCase):
    def test_queue_holds_many_records(self):
        n = const("REMOTE_TELEMETRY_SLOTS")
        self.assertIsNotNone(n, "no REMOTE_TELEMETRY_SLOTS: the send queue is still 2 slots")
        self.assertGreaterEqual(n, 64)
        self.assertIn("rtel::slotFree(remoteTelemetrySlots, remoteTelemetrySlotCount)", SRC)
        self.assertIn("rtel::slotOldest(remoteTelemetrySlots, remoteTelemetrySlotCount)", SRC)

    def test_capture_goes_on_while_backing_off(self):
        b = body("static void remoteTelemetryUpdateActive()")
        want = b[:b.index("if (want")]
        self.assertIn("SVC_BACKOFF", want, "capture pauses while the upload backs off: records are lost")
        self.assertIn("remoteTelemetrySlotCount > 1", want, "capture during backoff must depend on the PSRAM queue")

    def test_skip_is_retried_soon_without_a_backoff_step(self):
        b = body("static void remoteTelemetryBackoff(")
        self.assertIn("REMOTE_TELEMETRY_SKIP_RETRY_MS", b)
        self.assertRegex(b, r"if \(!skipped && remoteTelemetryBackoffStep < 255\)")
        self.assertLessEqual(const("REMOTE_TELEMETRY_SKIP_RETRY_MS") or 10 ** 9, 60000)

    def test_task_stack_and_slot_table_in_psram(self):
        """poc.23 (Robert 2026-11-19: BLE and remote telemetry may run from PSRAM)."""
        init = body("static bool remoteTelemetryInitCore()")
        self.assertIn("platformTaskCreatePinnedPsramStack(remoteTelemetryTask", init)
        self.assertIn("platformPsramAllocOnce(sizeof(rtel::Slot) * REMOTE_TELEMETRY_SLOTS)", init)
        self.assertNotRegex(SRC, r"static rtel::Slot remoteTelemetrySlots\[")

    def test_task_writes_no_flash(self):
        """A PSRAM-stack task must not write flash: the config write goes to remoteTelemetryLoop()."""
        task = body("static void remoteTelemetryTask(void *) {")
        blocked = body("static void remoteTelemetryBlocked()")
        for b in (task, blocked):
            self.assertNotIn("remoteTelemetryWriteConfig()", b)
            self.assertNotIn("LittleFS.open", b)
            self.assertNotIn("Preferences", b)
        self.assertIn("remoteTelemetryWriteConfig();", body("void remoteTelemetryLoop()"))

    def test_blocked_pauses_and_never_turns_the_opt_in_off(self):
        """poc.24 (Robert 2026-11-19): the collection point can never turn remote telemetry off; 'blocked' pauses."""
        b = body("static void remoteTelemetryBlocked()")
        self.assertNotIn("remoteTelemetryCfg.enabled = false", b)
        self.assertNotIn("remoteTelemetryCfgWritePending = true", b)
        self.assertNotIn("SVC_OFF", b)
        self.assertIn("remoteTelemetrySvc = SVC_BACKOFF;", b)   # poc.27: capture goes on into the queue until it is full
        self.assertNotIn("SVC_UID_TAKEN", b)
        self.assertIn("rtel::BACKOFF_PAUSE_MAX_MS", b)   # paused: spread, up to 60 min, resumes once the block is lifted

    def test_full_queue_pauses_capture_with_a_marker(self):
        """poc.24: when the backend stays unreachable the capture pauses (EV_PAUSE) instead of losing records."""
        b = body("static void remoteTelemetryUpdateActive()")
        self.assertIn("freeSlots >= 2", b)
        self.assertIn("send queue full", b)
        self.assertIn("EV_PAUSE", b)
        # poc.27 (Robert): a pause keeps the queue; it is delivered once the backend takes data again
        pause = b[b.index("} else if (!want && remoteTelemetryActive) {"):]
        self.assertNotIn(".busy = false", pause, "the pause throws the send queue away")
        self.assertNotIn("lostRecords +=", pause)

    def test_at_most_30_uploads_a_minute(self):
        """poc.27 (Robert): never more than 30 uploads a minute; a queue filled during a pause drips out."""
        self.assertGreaterEqual(const("REMOTE_TELEMETRY_UPLOAD_GAP_MS") or 0, 2000)
        task = body("static void remoteTelemetryTask(void *) {")
        self.assertIn("(uint32_t)(millis() - remoteTelemetryLastUploadMs) >= REMOTE_TELEMETRY_UPLOAD_GAP_MS", task)
        self.assertNotIn("if (remoteTelemetryLocalOn) q->busy = false;", task, "a failed upload drops the record")

    def test_guard_limits(self):
        self.assertEqual(const("REMOTE_TELEMETRY_MIN_FREE"), 12288)
        self.assertEqual(const("REMOTE_TELEMETRY_MIN_BLOCK"), 4096)


if __name__ == "__main__":
    unittest.main(verbosity=2)
