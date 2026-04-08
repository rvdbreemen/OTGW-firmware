---
# METADATA
Document Title: OpenTherm v4.2 Deep Audit — Archive README
Review Date: 2026-04-07 22:14:33 UTC
Status: COMPLETE
---

# OpenTherm v4.2 Deep Audit Archive

**Date:** 2026-04-07  
**Reviewer:** GitHub Copilot Advanced Agent  
**Scope:** Complete cross-reference of OpenTherm Protocol Specification v4.2 vs firmware implementation

## Documents

| Document | Description |
|----------|-------------|
| [AUDIT_REPORT.md](AUDIT_REPORT.md) | Full audit — frame format, codecs, all 128 IDs, status bits, code quality |

## Summary

The firmware achieves **high spec conformance** against OpenTherm v4.2.

All critical issues were addressed by the 2026-02-15 compliance review.
This audit found one incorrect comment (ID 38) which was corrected, and
documented three remaining minor gaps as design trade-offs.

## Code Change

**OTGW-Core.h — ID 38 comment** corrected: the f8.8 implementation is the
*correct* spec-conformant choice per OT v4.2 §5.3; the old comment incorrectly
stated it was a legacy compatibility measure.

## Prior Reviews Referenced

- [2026-02-15_opentherm-v42-compliance](../2026-02-15_opentherm-v42-compliance/) — Previous compliance review that fixed all critical issues
