#!/usr/bin/env python3
"""TASK-1699: the HA discovery drip sends one discovery config per tick, not one OT id per tick.

Robert 2026-11-07: "drip out the HA discovery on boot. One mqtt discovery message at a time and pace it
1 second apart". Before this task one drip tick ran doAutoConfigureMsgid(id) for a whole id; id 0 alone
streams the status-bit binary sensors, two climate entities, every SAT switch, select and number, and the
retired SAT clears in one tick (beta.123 diag: MQTT outbox high-water 48-49 after every boot).

The harness test/host/test_ha_discovery_json.py compiles the REAL composers (MQTTHaDiscovery.cpp verbatim)
with the slices of MQTTstuff.ino they need, including doAutoConfigureMsgidStep() when the revision has it
(part F). Per id under four settings permutations the drip is run to the end next to the full pass.

Checked:
  per tick    at most one non-empty config per drip step (a retired-entity clear may add a second, empty one);
  complete    the drip sends exactly the full pass's publishes (same topics and payloads), so every entity
              still appears in Home Assistant;
  verdict     the drip's done/retry verdict equals the full pass's return value (id 244 all-or-nothing);
  device      the full device block (ADR-140) rides the first step of id 0;
  interrupt   a drip of id 0 cut off by another id restarts id 0 and still sends every id-0 publish;
  drop        a config that fails while the publish gate closes is retried (no Done), the rest still goes out;
  shift       a zone that stops, or bSeparateSources switched off, during an id's drip skips no config.

Not covered here: loopMQTTDiscovery() itself (timer, pending bitmap, dripDeviceInfoPending); the bench run does.

Red on the old code: the revision has no step, so one drip tick is the full doAutoConfigureMsgid(0) call; the
test reports how many configs that one tick sends and fails.

Run:  python tests/test_discovery_drip_per_entity.py
Red:  OTGW_DISC_REV=HEAD~1 python tests/test_discovery_drip_per_entity.py   (a revision before TASK-1699)
"""
import os
import sys
import unittest
from collections import Counter
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "test" / "host"))
import test_ha_discovery_json as H   # noqa: E402

REV = os.environ.get("OTGW_DISC_REV") or None


class DiscoveryDripPerEntity(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        root = H.GEN / "ha_discovery_json" / ("drip_old" if REV else "drip")
        H.export(REV, root)
        cls.ctx, cls.calls, cls.pubs = H.build_and_run(f"TASK-1699 ({REV or 'working tree'})", root, REV)
        cls.drip = [d for d in H.LAST_DRIP if d["kind"] == "DRIP"]
        cls.interrupt = [d for d in H.LAST_DRIP if d["kind"] == "DRIPINT"]

    def old_tick_configs(self):
        """Configs one drip tick sends on a revision without the step: the full call for id 0."""
        seq = [c["seq"] for c in self.calls if c["tag"] == "doAutoConfigureMsgid" and c["args"].endswith(",id=0")]
        return Counter(p["seq"] for p in self.pubs if p["seq"] in seq and p["raw"])

    def test_one_config_per_tick(self):
        if not self.drip:
            per = self.old_tick_configs()
            self.fail(f"no per-entity drip step in this revision: one drip tick of id 0 sends "
                      f"{max(per.values()) if per else 0} configs (per permutation: {sorted(per.values())})")
        worst = max(self.drip, key=lambda d: int(d["max_cfg"]))
        self.assertLessEqual(int(worst["max_cfg"]), 1, f"a drip step sent more than one config: {worst}")
        self.assertLessEqual(max(int(d["max_pubs"]) for d in self.drip), 2,
                             "a drip step sent more than a config plus one empty retained clear")
        id0 = [d for d in self.drip if d["id"] == "0"]
        print(f"\n  id 0 drip steps per permutation: {[(d['perm'], d['steps']) for d in id0]}")
        print(f"  publishes over all ids, first permutation: "
              f"{sum(int(d['drip']) for d in self.drip if d['perm'] == self.drip[0]['perm'])}")

    def test_drip_sends_every_publish_of_the_full_pass(self):
        self.assertTrue(self.drip, "no per-entity drip step in this revision")
        bad = [d for d in self.drip if d["same"] != "1"]
        self.assertEqual(bad, [], "the drip sent a different set of topics/payloads than the full pass")

    def test_verdict_matches_full_pass(self):
        self.assertTrue(self.drip, "no per-entity drip step in this revision")
        bad = [d for d in self.drip if d["ret"] != d["full_ret"]]
        self.assertEqual(bad, [], "drip verdict differs from doAutoConfigureMsgid()")

    def test_device_block_on_first_step(self):
        self.assertTrue(self.drip, "no per-entity drip step in this revision")
        id0 = [d for d in self.drip if d["id"] == "0"]
        self.assertTrue(id0)
        self.assertEqual({d["dev_step"] for d in id0}, {"0"}, f"full device block not in step 0: {id0}")
        later = [d for d in self.drip if d["id"] != "0" and d["dev_step"] != "-1"]
        self.assertEqual(later, [], "a later id carried the full device block")

    def test_interrupted_drip_completes(self):
        # Another id takes the cursor, so id 0 restarts at its first config: repeats, never a gap.
        self.assertTrue(self.interrupt, "no per-entity drip step in this revision")
        self.assertEqual(self.interrupt[0]["complete"], "1", self.interrupt[0])

    def test_broker_drop_mid_id_waits_and_resumes(self):
        # A config fails while the publish gate closes: Retry (id not judged), then resume at that config.
        drop = [d for d in H.LAST_DRIP if d["kind"] == "DRIPDROP"]
        self.assertTrue(drop, "no per-entity drip step in this revision")
        d = drop[0]
        self.assertGreaterEqual(int(d["retries"]), 3, d)
        self.assertEqual((d["complete"], d["success"]), ("1", "1"), d)

    def test_slot_numbers_that_change_mid_id_skip_nothing(self):
        shift = [d for d in H.LAST_DRIP if d["kind"] == "DRIPSHIFT"]
        self.assertTrue(shift, "no per-entity drip step in this revision")
        d = shift[0]
        self.assertEqual(d["zones_covered"], "1", f"a zone that stops mid-drip made the drip skip configs: {d}")
        self.assertNotEqual(d["sources_id"], "-1", d)
        self.assertEqual(d["sources_covered"], "1", f"bSeparateSources off mid-drip made the drip skip configs: {d}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
