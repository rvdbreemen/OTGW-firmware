# Feature Investigations

This directory contains detailed investigations into feature requests and technical capabilities of the OTGW-firmware.

## Purpose

When a feature request is received, we may need to investigate:
- Whether the OpenTherm protocol supports the feature
- Whether the OTGW PIC firmware provides the necessary commands
- Whether the hardware supports the requested functionality
- Technical limitations and alternative solutions

## Investigations

### [COOL Command Investigation](COOL_COMMAND_INVESTIGATION.md)

**Date:** 2026-02-16  
**Request:** Add MQTT set command for cooling enable control  
**Status:** ❌ Not Feasible  
**Reason:** OTGW PIC firmware lacks command for cooling control

**Summary:**
- ✅ OpenTherm protocol supports cooling
- ✅ ESP8266 firmware monitors cooling status
- ❌ PIC firmware has no command to control cooling enable bit
- **Recommendation:** Request feature from Schelte Bron or use cooling-capable thermostat

---

## Investigation Template

When creating a new investigation, include:

1. **Executive Summary**
   - Feature request description
   - Feasibility assessment (✅/⚠️/❌)
   - Key finding in 1-2 sentences

2. **Detailed Analysis**
   - Protocol support (OpenTherm specification)
   - Hardware capabilities (OTGW PIC firmware)
   - Current implementation status
   - Technical limitations

3. **Alternative Solutions**
   - Workarounds
   - Architectural changes
   - External solutions

4. **Conclusion**
   - Clear recommendation
   - Action items
   - References

5. **Appendix**
   - Example responses for issue tracker
   - Code snippets
   - Technical details
