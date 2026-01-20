# Quick Reference Card: dev vs dev-rc4-branch

## 🎯 TL;DR

**dev wins** - Use it (after fixing 1 critical bug)

## 📊 By The Numbers

```
Code Change:     -5,159 lines (89% reduction)
Files Changed:   54
Commits:         6
Risk Level:      🟡 MEDIUM (after fix: 🟢 LOW)
Time to Fix:     5 minutes
Time to Test:    30 minutes
Production Ready: After critical fix ✅
```

## 🔴 Critical Issue (FIX NOW)

**Buffer Overrun in PIC Flashing**

```cpp
File: src/libraries/OTGWSerial/OTGWSerial.cpp
Line: ~309

❌ CURRENT (UNSAFE):
strncmp_P((char *)datamem + ptr, banner1, bannerLen)

✅ FIX TO:
memcmp_P((char *)datamem + ptr, banner1, bannerLen)
```

**Why**: `strncmp_P()` reads past buffer on binary data → Exception (2) crash  
**When**: During PIC firmware flashing  
**Fix Time**: 5 minutes  

## 🟡 Medium Priority

**Test MQTT Auto-Discovery** (30 min)
- May truncate messages >128 bytes
- Test with multiple sensors
- If fails: Add `MQTTclient.setBufferSize(512);`

## ✅ What Changed (The Good Stuff)

| Feature | Before | After | Benefit |
|---------|--------|-------|---------|
| Heap monitoring | 4 levels | None | Simpler |
| MQTT buffer | 1350 bytes | 128 bytes | Cleaner |
| WebSocket limits | Max 3 clients | No limit | More flexible |
| Code complexity | HIGH | LOW | Maintainable |
| PROGMEM usage | Everywhere | Selective | Readable |

## 🏆 Why dev Wins

1. **89% less code** to test/debug/maintain
2. **No throttling** = better performance
3. **Proven patterns** = less risk
4. **Simpler architecture** = easier to extend

## ⚠️ What to Watch

- [ ] Fix buffer overrun (🔴 CRITICAL)
- [ ] Test MQTT discovery (🟡 HIGH)
- [ ] Review null pointer usage (🟡 MEDIUM)
- [ ] Run regression tests (🟢 REQUIRED)

## 📋 Pre-Merge Checklist

```bash
# 1. Fix buffer overrun (5 min)
vim src/libraries/OTGWSerial/OTGWSerial.cpp
# Change strncmp_P to memcmp_P at line ~309

# 2. Test MQTT auto-discovery (30 min)
# - Enable MQTT
# - Add multiple sensors
# - Restart Home Assistant
# - Verify all entities discovered

# 3. Regression test (4 hours)
# - Core functionality
# - MQTT integration
# - PIC flashing
# - Sensors
# - WebSocket

# 4. Merge
git checkout main
git merge dev
git push
```

## 🚀 Migration Path

**From dev-rc4-branch → dev**

1. Backup: `curl http://<ip>/api/v1/settings > backup.json`
2. Flash: Upload firmware via Web UI
3. Verify: Check `/api/v1/health` returns `"status": "UP"`
4. Test: MQTT, WebSocket, sensors

**Rollback if needed**: Just flash previous firmware

## 📖 Full Documentation

- **Quick Start**: [EXECUTIVE_SUMMARY.md](EXECUTIVE_SUMMARY.md)
- **Full Analysis**: [BRANCH_COMPARISON_REPORT.md](BRANCH_COMPARISON_REPORT.md)
- **Navigation**: [README_COMPARISON.md](README_COMPARISON.md)

## 💡 Decision Tree

```
Are you deciding which branch to use?
├─ YES → Use dev (after fixing buffer overrun)
└─ NO
   ├─ Need details? → Read BRANCH_COMPARISON_REPORT.md
   ├─ Need migration help? → Read MIGRATION_GUIDE.md
   └─ Need features? → Read FEATURE_COMPARISON_MATRIX.md
```

## 🎓 Key Lesson

**Simplicity wins**. The complex heap management in dev-rc4-branch solved theoretical problems that don't occur in practice. The dev branch trusts the ESP8266 platform to manage its own memory - and it works better.

---

**Generated**: 2026-01-20 23:50:00 UTC  
**Verdict**: ✅ dev (after critical fix)  
**Confidence**: HIGH  
