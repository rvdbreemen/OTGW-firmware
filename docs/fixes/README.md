# Bug Fixes Documentation

This directory contains detailed documentation for bug fixes in the OTGW-firmware project.

## Available Fix Documents

### [MQTT Whitespace Authentication Fix](mqtt-whitespace-auth-fix.md)
- **Issue**: MQTT authentication failures after upgrading to v1.0 with "addons" user
- **Root Cause**: Leading/trailing whitespace in MQTT credentials not being trimmed
- **Fix**: Added automatic whitespace trimming when reading and updating MQTT credentials
- **Date**: 2026-02-10
- **Commit**: eba5d51

## Purpose

These documents provide:
- Detailed root cause analysis
- Step-by-step fix implementation
- Testing procedures
- Migration notes for users
- Related code references

## Contributing

When adding new bug fix documentation:
1. Create a new markdown file with a descriptive name
2. Include sections: Issue Description, Root Cause, The Fix, Testing, Impact, Related Files, Commit
3. Update this README with a link and summary
4. Reference the fix in release notes
