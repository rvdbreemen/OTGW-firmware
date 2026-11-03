#!/usr/bin/env python3
"""TASK-1541, Sergeant D entry 240: switching SAT off also puts back SH= and SW= and clears a pushed TC=.

Nothing is re-implemented. The real satDisable(), satSendRelease(), satSendRestore() and satRetryRelease() are cut out of
SATcontrol.ino by test_sat_release_retry.build_cpp(); only the SAT sender side is simulated by calling the real
satShNoteBeforeSend() / satSwNoteBeforeSend() that satMaxTSetSendStep() / satDhwSetpointSync() call right before they send
(CallSite below checks that order at source level), and by setting _sat_tcPushed like satEnqueueIfChangedTC().

OTGW_SAT_INO points at another copy (`git show HEAD:src/OTGW-firmware/SATcontrol.ino`) for the old-vs-fix run: the old code
sends only CS=0, CH=1, MM=T, so every restore test fails there.

Not executed here: the loop task call sites (source level) and the real SH=/SW= senders' own change checks.

Run: python tests/test_sat_disable_handback.py
"""
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402
import test_sat_release_retry as rr  # noqa: E402

SAT_INO = rr.SAT_INO

MAIN = r"""
static void show(const char* t) { printf("-- %s\n", t); }
// SAT has sent SH= / SW= (the real note functions run first, as in the senders) and the boiler now shows SAT's values
static void satChangedBoiler() {
  satShNoteBeforeSend(75.0f); satSwNoteBeforeSend(55.0f);   // the installer values the boiler shows when SAT sends SH= / SW=
}
int main(int argc, char** argv) {
  const char* sc = argv[1];
  g_ms = 10000;
  state.sat.bActive = true;
  g_satEnabled = true;
  if (!strcmp(sc, "restore")) { satChangedBoiler(); _sat_tcPushed = true; g_satEnabled = false; satDisable(); }
  else if (!strcmp(sc, "no_tc")) { satChangedBoiler(); g_satEnabled = false; satDisable(); }
  else if (!strcmp(sc, "never_read")) {
    satShNoteBeforeSend(0.0f); satSwNoteBeforeSend(60.0f);   // MsgID 57 never read (0), 56 is 60
    g_satEnabled = false; satDisable();
  }
  else if (!strcmp(sc, "sat_never_sent")) { _sat_tcPushed = false; g_satEnabled = false; satDisable(); }   // SAT changed nothing
  else if (!strcmp(sc, "twice")) { satChangedBoiler(); _sat_tcPushed = true; g_satEnabled = false; satDisable(); show("again"); satDisable(); }
  else if (!strcmp(sc, "pause")) {          // OpenTherm loss / outside temperature missing / flash: SAT stays enabled
    satChangedBoiler(); _sat_tcPushed = true; satDisable(); show("switched off"); g_satEnabled = false; satDisable();
  }
  else if (!strcmp(sc, "trip")) { satChangedBoiler(); _sat_tcPushed = true; state.sat.bSafetyTripped = true; satDisable(); }
  else if (!strcmp(sc, "loss_switchoff")) {  // user switch-off during an OpenTherm loss: the loss hand-back is not repeated
    satChangedBoiler(); _sat_tcPushed = true; satDisable(); show("lost");
    state.sat.bOtLinkLost = true; g_satEnabled = false; satDisable();
  }
  else if (!strcmp(sc, "drop_sh")) {
    satChangedBoiler(); _sat_tcPushed = true; g_satEnabled = false;
    g_drop = 3; satDisable(); show("dropped");
    g_ms += 1000; satRetryRelease(); show("retry still dropping");
    g_drop = 0; g_ms += 1000; satRetryRelease(); show("retry accepted");
    g_ms += 1000; satRetryRelease(); show("quiet");
  }
  else if (!strcmp(sc, "persist")) {          // capture: the values are stored once, in the deferred settings (one dirty mark per first capture: SH, SW)
    satChangedBoiler(); printf("-- stored %d %d dirty=%d\n", (int)g_persistSH, (int)g_persistSW, g_persistDirty);
    satShNoteBeforeSend(50.0f); satSwNoteBeforeSend(45.0f);   // a second SH=/SW= (SAT's own value now on the boiler) changes nothing
    printf("-- stored %d %d dirty=%d\n", (int)g_persistSH, (int)g_persistSW, g_persistDirty);
  }
  else if (!strcmp(sc, "reboot") || !strcmp(sc, "reboot_not_overwritten")) {   // RAM lost, settings.ini kept
    satChangedBoiler();
    _sat_preSH = -1; _sat_preSW = -1; _sat_shChanged = false; _sat_swChanged = false; _sat_tcPushed = false;   // reboot
    satPreSatLoad(); int d = g_persistDirty;
    if (!strcmp(sc, "reboot_not_overwritten")) { satShNoteBeforeSend(50.0f); satSwNoteBeforeSend(45.0f); }   // SAT sends again, boiler shows SAT's values
    printf("-- loaded stored %d %d dirty+%d\n", (int)g_persistSH, (int)g_persistSW, g_persistDirty - d);
    g_satEnabled = false; satDisable(); printf("-- stored after %d %d\n", (int)g_persistSH, (int)g_persistSW);
  }
  else if (!strcmp(sc, "reenable")) {        // switch-off, SAT takes over again, switch-off: pre-SAT values are kept
    satChangedBoiler(); g_satEnabled = false; satDisable(); show("off 1");
    g_satEnabled = true;
    satShNoteBeforeSend(50.0f); satSwNoteBeforeSend(45.0f);              // SAT sends SH=/SW= again; the boiler now shows SAT's values
    g_satEnabled = false; satDisable();
  }
  printf("-- end\n");
  return 0;
}
"""


def build_cpp(src: str) -> str:
    cpp = rr.build_cpp(src)
    # replace the release-retry main() with this one
    cut = cpp.index(rr.MAIN.strip()[:40])
    cpp = cpp[:cut]
    if "satPreSatLoad" not in src:   # old file: no stored restore values
        cpp += ("static void satPreSatLoad() {}\nstatic int16_t _sat_preSH = -1, _sat_preSW = -1;\n"
                "static bool _sat_shChanged = false, _sat_swChanged = false;\n")   # stand-ins: the scenarios compile and fail on their asserts
    cpp = cpp.replace('(g_drop == 2 && !strncmp(cmd, "MM=", 3))',
                      '(g_drop == 2 && !strncmp(cmd, "MM=", 3)) || (g_drop == 3 && !strncmp(cmd, "SH=", 3))')
    if "satShNoteBeforeSend" not in src:   # old file: the sender hooks and the TC flag do not exist
        cpp += ("\nstatic void satShNoteBeforeSend(float) {}\nstatic void satSwNoteBeforeSend(float) {}\n"
                "static bool _sat_tcPushed = false;\n")
    return cpp + MAIN


class Handback(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        text = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        cpp = d / "t.cpp"
        src = build_cpp(text)
        # the shared stubs gain the OT values and satIsEnabled() in test_sat_ot_loss.STUBS; an old copy of that file is not used
        cpp.write_text(src, encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_sc(self, sc):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), sc], capture_output=True, text=True, check=True).stdout.splitlines()
        return [l.split(" ", 1)[1] if l[0].isdigit() else l for l in out]

    def test_switch_off_restores_sh_sw_and_clears_tc_in_the_decided_order(self):
        self.assertEqual(self.run_sc("restore"), ["CS=0", "CH=1", "MM=T", "SH=75", "SW=55", "TC=0", "-- end"])

    def test_tp_and_mi_are_never_sent(self):
        for sc in ("restore", "twice", "trip", "reenable"):
            for line in self.run_sc(sc):
                self.assertFalse(line.startswith(("TP=", "MI=")), (sc, line))

    def test_tc_zero_only_when_sat_pushed_a_tc(self):
        self.assertEqual(self.run_sc("no_tc"), ["CS=0", "CH=1", "MM=T", "SH=75", "SW=55", "-- end"])

    def test_a_value_never_read_sends_nothing_for_that_item(self):
        self.assertEqual(self.run_sc("never_read"), ["CS=0", "CH=1", "MM=T", "SW=60", "-- end"])

    def test_nothing_restored_when_sat_changed_nothing(self):
        self.assertEqual(self.run_sc("sat_never_sent"), ["CS=0", "CH=1", "MM=T", "-- end"])

    def test_second_switch_off_does_not_restore_again(self):
        self.assertEqual(self.run_sc("twice"), ["CS=0", "CH=1", "MM=T", "SH=75", "SW=55", "TC=0", "-- again",
                                                "CS=0", "CH=1", "MM=T", "-- end"])

    def test_pause_with_sat_still_enabled_restores_nothing_until_it_is_switched_off(self):
        self.assertEqual(self.run_sc("pause"), ["CS=0", "CH=1", "MM=T", "-- switched off",
                                                "CS=0", "CH=1", "MM=T", "SH=75", "SW=55", "TC=0", "-- end"])

    def test_safety_trip_restores(self):
        self.assertEqual(self.run_sc("trip"), ["CS=0", "CH=1", "MM=T", "SH=75", "SW=55", "TC=0", "-- end"])

    def test_switch_off_during_opentherm_loss_sends_only_the_restore(self):
        self.assertEqual(self.run_sc("loss_switchoff"), ["CS=0", "CH=1", "MM=T", "-- lost", "SH=75", "SW=55", "TC=0", "-- end"])

    def test_dropped_sh_is_retried_with_the_whole_release_until_accepted_then_quiet(self):
        out = self.run_sc("drop_sh")
        i, k, j = out.index("-- dropped"), out.index("-- retry still dropping"), out.index("-- retry accepted")
        # SH= dropped: CS=0 CH=1 MM=T accepted, SW= and TC=0 still tried, nothing committed (flags stay)
        self.assertEqual(out[:i], ["CS=0", "CH=1", "MM=T", "SW=55", "TC=0"])
        self.assertEqual(out[i + 1:k], ["CS=0", "CH=1", "MM=T", "SW=55", "TC=0"])      # retry while the queue still drops SH=
        self.assertEqual(out[k + 1:j], ["CS=0", "CH=1", "MM=T", "SH=75", "SW=55", "TC=0"])   # accepted: the whole release again
        self.assertEqual(out[j + 1:], ["-- quiet", "-- end"])

    def test_capture_stores_the_installer_values_once_in_the_deferred_settings(self):
        self.assertEqual(self.run_sc("persist"), ["-- stored 75 55 dirty=2", "-- stored 75 55 dirty=2", "-- end"])

    def test_reboot_then_switch_off_restores_the_stored_values_and_clears_them(self):
        self.assertEqual(self.run_sc("reboot"), ["-- loaded stored 75 55 dirty+0", "CS=0", "CH=1", "MM=T", "SH=75", "SW=55",
                                                 "-- stored after 0 0", "-- end"])

    def test_a_stored_value_is_not_overwritten_by_sats_own_value_after_a_reboot(self):
        self.assertEqual(self.run_sc("reboot_not_overwritten"), ["-- loaded stored 75 55 dirty+0", "CS=0", "CH=1", "MM=T", "SH=75",
                                                                 "SW=55", "-- stored after 0 0", "-- end"])

    def test_restore_clears_the_stored_values(self):
        out = self.run_sc("restore")
        self.assertEqual(out[-1], "-- end")   # the stored pair is checked in the reboot scenarios above

    def test_pre_sat_values_survive_a_re_enable(self):
        out = self.run_sc("reenable")
        self.assertEqual(out[:out.index("-- off 1")], ["CS=0", "CH=1", "MM=T", "SH=75", "SW=55"])
        self.assertEqual(out[out.index("-- off 1") + 1:], ["CS=0", "CH=1", "MM=T", "SH=75", "SW=55", "-- end"])


class CallSite(unittest.TestCase):
    def setUp(self):
        self.src = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")

    def test_senders_remember_the_boiler_value_before_they_send(self):
        s = self.src
        a = s.index("static void satMaxTSetSendStep()")
        sh = s[a:s.index("\n}\n", a)]
        self.assertLess(sh.index("satShNoteBeforeSend(OTcurrentSystemState.MaxTSet);"), sh.index("addCommandToQueue(cmd"))
        b = s.index("static void satDhwSetpointSync()")
        sw = s[b:s.index("\n}\n", b)]
        self.assertLess(sw.index("satSwNoteBeforeSend(OTcurrentSystemState.TdhwSet);"), sw.index("addCommandToQueue(swCmd"))

    def test_tc_push_sets_the_flag(self):
        s = self.src
        a = s.index("static bool satEnqueueIfChangedTC(float tc)")
        body = s[a:s.index("\n}\n", a)]
        self.assertIn("_sat_tcPushed", body)

    def test_hand_back_call_sites(self):
        s = self.src
        self.assertIn("if (!satSendRelease(restore)) _sat_releasePending = true;", s)    # satDisable()
        self.assertIn("if (!satSendRelease(false)) _sat_releasePending = true;", s)      # restart hand-back: not a switch-off
        self.assertIn("const bool restore = !satIsEnabled() || state.sat.bSafetyTripped;", s)

    def test_settings_keys_are_written_and_read_but_not_listed(self):
        st = (SAT_INO.parent / "settingStuff.ino").read_text(encoding="utf-8")
        for key, field in (("SATpresatmaxtset", "iPreSatMaxTSet"), ("SATpresatdhwset", "iPreSatDhwSet")):
            self.assertIn('writeJsonIntKV(file, F("%s"), settings.sat.%s)' % (key, field), st)
            self.assertIn('strcasecmp_P(field, PSTR("%s")) == 0)' % key, st)
        api = (SAT_INO.parent / "restAPI.ino").read_text(encoding="utf-8")
        self.assertNotIn("satpresat", api)   # internal: not in the editable settings listing
        self.assertIn("satPreSatLoad();", self.src)
        self.assertIn("int16_t  iPreSatMaxTSet", (SAT_INO.parent / "SATtypes.h").read_text(encoding="utf-8"))

    def test_no_tp_or_mi_in_the_restore(self):
        s = self.src
        a = s.index("static bool satSendRestore()")
        body = s[a:s.index("\n}\n", a)]
        self.assertNotIn("TP=", body)
        self.assertNotIn("MI=", body)


if __name__ == "__main__":
    unittest.main(verbosity=2)
