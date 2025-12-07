# Response to Issue #259: Setting DHW Setpoint Not Working

## Summary

User David reports that he cannot set the DHW (Domestic Hot Water) setpoint on his **Cosmogas Q30B boiler** using the OTGW in Gateway mode. He can read the setpoint but cannot change it. With the original thermostat, he can set the DHW setpoint successfully.

## Root Cause

This is **NOT a firmware bug**. This is a **hardware limitation of the Cosmogas Q30B boiler** (and many other boiler models). The boiler only accepts DHW setpoint changes from an OpenTherm thermostat, not from the OTGW operating in standalone mode without a thermostat.

## Technical Details

### What's Happening

1. The OTGW firmware is correctly sending the `SW` command (OpenTherm Message ID 56 - DHW Setpoint)
2. The command follows the OpenTherm protocol specification
3. However, the boiler's firmware checks whether the command originates from a trusted thermostat
4. Without a thermostat, the boiler ignores or reverts the command

### OpenTherm Protocol Background

- **Message ID 6**: Remote Boiler Parameter flags indicate what can be changed
  - Bit HB0: DHW setpoint transfer-enable
  - Bit LB0: DHW setpoint read/write capability
- **Message ID 56**: DHW Setpoint (Remote parameter 1)

Even when the boiler reports that DHW setpoint is read/write capable, it may only accept changes from a thermostat, not from standalone gateway mode.

## Solution for the User

The user has **three main options**:

### Option 1: Use OTGW with Thermostat (Recommended) ✅

Connect the OTGW **between** the thermostat and boiler:
```
[Thermostat] <--OpenTherm--> [OTGW] <--OpenTherm--> [Boiler]
```

**Benefits:**
- The boiler accepts DHW setpoint changes (they appear to come from the thermostat)
- The OTGW can still monitor and modify all OpenTherm communication
- Remote control via MQTT/REST API works by having OTGW modify thermostat commands
- Full OpenTherm functionality maintained

### Option 2: Check Boiler Service Menu ⚙️

Some boilers have a service menu setting to enable "external control" or allow DHW setpoint changes without a thermostat. The user should:
- Check the Cosmogas Q30B service manual
- Contact Cosmogas technical support
- Ask if there's a setting to enable OpenTherm control without thermostat

### Option 3: Manual Control 📝

If options 1 and 2 are not feasible, the user must adjust the DHW setpoint directly on the boiler's control panel.

## What Has Been Done

I have created comprehensive documentation to address this issue:

### 1. DHW Setpoint Troubleshooting Guide
**File:** `docs/DHW-Setpoint-Troubleshooting.md`

Includes:
- Detailed explanation of the root cause
- Technical background on OpenTherm Remote Boiler Parameters
- Step-by-step troubleshooting
- Multiple workarounds and solutions
- How to verify if your boiler supports remote DHW setpoint changes
- Testing procedures
- MQTT and REST API examples

### 2. FAQ Document
**File:** `docs/FAQ.md`

Includes:
- Common questions about DHW setpoint issues
- Quick answers for other frequent problems
- Configuration examples
- Integration guides for Home Assistant, MQTT, etc.
- Troubleshooting steps for various issues

### 3. GitHub Issue Templates
**Files:** `.github/ISSUE_TEMPLATE/`

Created templates for:
- DHW Setpoint specific issues (guides users to documentation first)
- General bug reports
- Feature requests
- Config file with links to documentation and community

### 4. Updated README
**File:** `README.md`

Added quick links section pointing to:
- FAQ
- DHW Setpoint Troubleshooting Guide

### 5. Issue Response Template
**File:** `docs/ISSUE-RESPONSE-DHW-SETPOINT.md`

Template response that can be customized for responding to DHW setpoint issues.

## Suggested Response to Issue #259

```markdown
Hi @[username],

Thank you for reporting this issue. I understand the frustration of not being able to set the DHW setpoint on your Cosmogas Q30B boiler.

Unfortunately, this is **not a firmware bug** but a **hardware limitation of your boiler**. Many boilers, including the Cosmogas Q30B, only accept DHW setpoint changes from an OpenTherm thermostat and will ignore commands sent by the OTGW in standalone mode.

### Why This Happens

Your boiler's firmware requires that DHW setpoint commands originate from a proper OpenTherm thermostat. The OTGW firmware is working correctly - it's sending the proper OpenTherm `SW` command (Message ID 56). However, your boiler is choosing to ignore it because it didn't come from a thermostat.

### Recommended Solution

Use your OTGW in **gateway mode** between your thermostat and boiler:
```
[Thermostat] <--OpenTherm--> [OTGW] <--OpenTherm--> [Boiler]
```

This way:
- The boiler accepts DHW setpoint changes (they appear to come from the thermostat)
- You can still use MQTT/REST API to control the setpoint via the OTGW
- All OpenTherm functionality works as expected

### Documentation

I've created comprehensive documentation to help with this issue:
- **[DHW Setpoint Troubleshooting Guide](docs/DHW-Setpoint-Troubleshooting.md)** - Detailed explanation and workarounds
- **[FAQ](docs/FAQ.md)** - Common questions and answers

Please read the troubleshooting guide for detailed steps, verification procedures, and alternative solutions. It includes specific examples for testing and debugging your configuration.

If you discover a workaround specific to the Cosmogas Q30B (such as a service menu setting), please share it here so others can benefit!
```

## Additional Notes

### Known Affected Boilers
- Cosmogas Q30B (this issue)
- De Dietrich models
- Various Remeha models
- Many others (manufacturer dependent)

### What the OTGW Firmware Does Correctly
✅ Sends proper OpenTherm SW command  
✅ Follows OpenTherm protocol specification  
✅ Can read DHW setpoint from boiler  
✅ Works correctly when used with a thermostat  

### What Cannot Be Changed (Hardware Limitation)
❌ Cannot force boiler to accept commands without thermostat  
❌ Cannot override boiler's security/authentication requirements  
❌ Cannot change how boiler implements OpenTherm protocol  

## Related External Issues
- Home Assistant Core #38078 - Same issue with HA integration
- Multiple community forum posts about this limitation
- OTGW standalone operation documentation: https://otgw.tclcode.com/standalone.html

## Closing Recommendation

This issue should be **closed as "by design" or "won't fix"** with a reference to the documentation, as:
1. It's a boiler hardware limitation, not a firmware bug
2. The OTGW firmware is working correctly
3. Comprehensive documentation has been provided
4. The recommended solution (using with thermostat) is well established
5. This helps set expectations for future similar reports

However, the issue could be kept open as a **reference issue** that can be linked when other users report the same problem with their boilers.

---

**Files Changed:**
- `README.md` - Added documentation links
- `docs/DHW-Setpoint-Troubleshooting.md` - New comprehensive guide
- `docs/FAQ.md` - New FAQ document
- `docs/ISSUE-RESPONSE-DHW-SETPOINT.md` - Response template
- `.github/ISSUE_TEMPLATE/dhw-setpoint-issue.md` - Issue template
- `.github/ISSUE_TEMPLATE/bug_report.md` - Bug report template
- `.github/ISSUE_TEMPLATE/feature_request.md` - Feature request template
- `.github/ISSUE_TEMPLATE/config.yml` - Template configuration

All changes are documentation only - no code modifications needed.
