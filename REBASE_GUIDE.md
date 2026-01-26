# Rebase Guide: PIC Flashing Fix PR to Dev Branch

## Overview

This guide walks you through rebasing the `copilot/fix-pic-flashing-issue` branch onto the latest `dev` branch to incorporate recent changes and prepare for merging.

## Current State

**Your PR Branch**: `copilot/fix-pic-flashing-issue`
- Based on: commit `3ea1d59` (rc4)
- Contains: 17 commits with PIC flashing bug fixes and debug logging
- Status: Clean (temporary documentation already removed)

**Target Branch**: `dev`
- Current commit: `15095c7` (rc5)
- New commits since your branch started: 11 commits including:
  - Version bump to rc5
  - Browser compatibility fixes (#383, #382)
  - Device settings support
  - Additional PROGMEM refactoring

## Prerequisites

Before starting, ensure:
- [ ] You have no uncommitted changes (`git status` is clean)
- [ ] You have backed up your work (optional but recommended)
- [ ] You understand git rebase basics

## Rebase Strategy

We'll use **interactive rebase** to clean up the commit history while incorporating dev branch changes.

---

## Step-by-Step Instructions

### Step 1: Prepare Your Environment

```bash
# 1.1 Check your current status
git status
# Should show: "On branch copilot/fix-pic-flashing-issue" with no uncommitted changes

# 1.2 Fetch the latest dev branch
git fetch origin dev

# 1.3 View what commits you'll be rebasing
git log --oneline origin/dev..HEAD
# This shows your 17 commits that aren't in dev yet
```

**Expected output**: List of 17 commits starting from your earliest changes.

---

### Step 2: Start Interactive Rebase

```bash
# 2.1 Start the interactive rebase
git rebase -i origin/dev
```

**What happens**: Your default editor opens with a list of commits.

---

### Step 3: Edit the Rebase Plan

Your editor will show something like:

```
pick 8351d25 Add HTTP flush and debug logging to prevent crash
pick c6e64d9 Remove client().stop() call - was breaking connections
pick c002fd6 Remove DISABLE_WEBSOCKET dead code
pick e491578 Revert all changes to RC3 baseline for testing
pick 38717c6 Undo RC3 reversion - restore fixes from c6e64d9
pick 4451be3 Restore code to c6e64d9 state with all fixes
pick fd83f38 Remove yield() calls from OTGWSerial.cpp per user request
pick 345d4c9 Restore missing flush() call in OTGW-Core.ino
pick 48dab44 Fix PROGMEM access bug in OTGWSerial::matchBanner - use pgm_read_ptr for banners array
pick 005079d Fix sscanf format strings: Move from PROGMEM to RAM (sscanf doesn't support PROGMEM)
pick 33b27f8 Add comprehensive debug logging for PIC flashing (telnet and WebUI)
pick b4e5cc3 Clean up PR: Remove temporary analysis/debugging documentation files
# ... plus a few more
```

**Change it to** (keep only the valuable commits and squash related ones):

```
pick 345d4c9 Restore missing flush() call in OTGW-Core.ino
squash 48dab44 Fix PROGMEM access bug in OTGWSerial::matchBanner
squash 005079d Fix sscanf format strings: Move from PROGMEM to RAM
pick c002fd6 Remove DISABLE_WEBSOCKET dead code
pick 33b27f8 Add comprehensive debug logging for PIC flashing
pick b4e5cc3 Clean up PR: Remove temporary analysis/debugging documentation files

# DELETE or comment out (add # in front) all other lines
# These are temporary commits (reverts, re-reverts, etc.)
```

**Save and close** the editor (`:wq` in vim, `Ctrl+X` then `Y` in nano).

---

### Step 4: Edit Squashed Commit Message

Git will open your editor again for the squashed commit message. You'll see:

```
# This is a combination of 3 commits.
# This is the 1st commit message:

Restore missing flush() call in OTGW-Core.ino

# This is the commit message #2:

Fix PROGMEM access bug in OTGWSerial::matchBanner - use pgm_read_ptr for banners array

# This is the commit message #3:

Fix sscanf format strings: Move from PROGMEM to RAM (sscanf doesn't support PROGMEM)
```

**Replace with a clean, combined message**:

```
Fix PIC flashing: HTTP flush and PROGMEM bugs

Three critical bugs fixed:

1. Restored missing httpServer.client().flush() call
   - Merge commit b7113d0 accidentally removed it
   - Without flush(), ESP crashes when clicking flash button

2. Fixed PROGMEM pointer access in banners array
   - Changed to use pgm_read_ptr() for reading pointer arrays from PROGMEM
   - Prevents invalid memory access during bootloader detection

3. Moved sscanf format strings from PROGMEM to RAM
   - sscanf() has no _P variant and cannot read format strings from flash
   - Prevents undefined behavior during hex file parsing

Files changed:
- OTGW-Core.ino
- src/libraries/OTGWSerial/OTGWSerial.cpp
```

**Save and close** the editor.

---

### Step 5: Handle Conflicts (If Any)

If git encounters conflicts during rebase:

```bash
# Git will pause and show:
# CONFLICT (content): Merge conflict in <filename>
# error: could not apply <commit>...

# 5.1 Check which files have conflicts
git status
# Look for "both modified:" entries

# 5.2 Open each conflicted file in an editor
# Look for conflict markers:
# <<<<<<< HEAD
# (dev branch version)
# =======
# (your changes)
# >>>>>>> commit message

# 5.3 Resolve each conflict by:
# - Keeping your changes if they're bug fixes
# - Keeping dev changes if they're newer features
# - Combining both if needed
# - Remove the conflict markers (<<<, ===, >>>)

# 5.4 After resolving all conflicts in a file:
git add <resolved-file>

# 5.5 Continue the rebase
git rebase --continue

# 5.6 Repeat steps 5.2-5.5 for each commit that has conflicts
```

**Common conflicts to expect**:
- `version.h` - version number (keep rc5 from dev)
- `OTGW-Core.ino` - if dev has new features in same functions
- `OTGWSerial.cpp` - if dev has other PROGMEM fixes

**Conflict resolution tips**:
- For version conflicts: Always keep the dev branch version (rc5)
- For code conflicts: Keep your bug fixes but preserve dev's new features
- When in doubt: Keep both changes and test after rebase

---

### Step 6: Verify the Rebase

```bash
# 6.1 Check the commit history
git log --oneline origin/dev..HEAD
# Should now show only 4-5 clean commits

# 6.2 Check what changed
git diff origin/dev
# Review the final changes to ensure they're correct

# 6.3 Verify no files were lost
git status
# Should show: "Your branch and 'origin/copilot/fix-pic-flashing-issue' have diverged"
```

**Expected commits after rebase**:
1. Fix PIC flashing: HTTP flush and PROGMEM bugs
2. Remove DISABLE_WEBSOCKET dead code
3. Add comprehensive debug logging for PIC flashing
4. Clean up PR: Remove temporary analysis/debugging documentation files

---

### Step 7: Force Push to Update PR

⚠️ **WARNING**: This will rewrite the PR history. Make sure the rebase is correct first!

```bash
# 7.1 Force push with lease (safer than --force)
git push --force-with-lease origin copilot/fix-pic-flashing-issue

# If this fails with "stale info", someone else pushed to your branch
# In that case, use regular force push:
# git push --force origin copilot/fix-pic-flashing-issue
```

**What happens**: Your PR will be updated with the new, cleaner commit history based on the latest dev branch.

---

### Step 8: Verify the PR

1. **Go to GitHub**: https://github.com/rvdbreemen/OTGW-firmware/pull/XXX
2. **Check the "Commits" tab**: Should show 4-5 clean commits
3. **Check the "Files changed" tab**: Should show only your bug fixes and debug logging
4. **Verify no merge conflicts**: GitHub should show "No conflicts with base branch"
5. **Check CI/CD**: Wait for automated builds to pass

---

## Troubleshooting

### Problem: "Cannot rebase: You have unstaged changes"

```bash
# Stash your changes temporarily
git stash

# Try the rebase again
git rebase -i origin/dev

# After successful rebase, restore your changes
git stash pop
```

### Problem: "error: could not apply [commit]"

This means there's a conflict. Follow **Step 5** above to resolve it.

### Problem: Too many conflicts, want to start over

```bash
# Abort the rebase
git rebase --abort

# You're back to where you started
# Try a different strategy or ask for help
```

### Problem: Accidentally dropped important commits

```bash
# Find the commit SHA from before rebase
git reflog

# Look for the line that says "rebase -i (start)"
# The line above it shows your branch before rebase

# Reset to that commit
git reset --hard <commit-sha>

# Try the rebase again
```

### Problem: Push rejected after rebase

```bash
# Use force-with-lease (safer)
git push --force-with-lease origin copilot/fix-pic-flashing-issue

# If that fails, use regular force push
git push --force origin copilot/fix-pic-flashing-issue
```

---

## Alternative: Simple Rebase (No Squashing)

If interactive rebase seems too complex, you can do a simple rebase:

```bash
# 1. Rebase without editing commits
git rebase origin/dev

# 2. Resolve conflicts as they appear (see Step 5 above)

# 3. Force push when done
git push --force-with-lease origin copilot/fix-pic-flashing-issue
```

This keeps all 17 commits but updates them to be based on the latest dev branch.

---

## After Rebase: Testing

Before requesting final review:

1. **Build the firmware locally** (if possible):
   ```bash
   python build.py
   ```

2. **Check for any new compiler warnings** related to your changes

3. **Review the final diff**:
   ```bash
   git diff origin/dev
   ```

4. **Ensure all your fixes are still present**:
   - HTTP flush() call in OTGW-Core.ino
   - PROGMEM fixes in OTGWSerial.cpp
   - Debug logging throughout

---

## Summary

**What this rebase accomplishes**:
- ✅ Updates your PR to be based on the latest dev branch (rc5)
- ✅ Incorporates 11 new commits from dev
- ✅ Cleans up commit history (17 commits → 4-5 clean commits)
- ✅ Resolves any conflicts between your changes and dev
- ✅ Makes the PR ready for final review and merge

**Final commit structure** (after rebase):
1. **Fix PIC flashing: HTTP flush and PROGMEM bugs** - Main bug fixes
2. **Remove DISABLE_WEBSOCKET dead code** - Code cleanup
3. **Add comprehensive debug logging for PIC flashing** - Diagnostics
4. **Clean up PR: Remove temporary analysis files** - Documentation cleanup

---

## Need Help?

If you encounter issues during the rebase:
1. Don't panic - you can always abort with `git rebase --abort`
2. Take a screenshot of the error message
3. Use `git reflog` to see your recent git history
4. Ask for help with specific error messages

Good luck with your rebase! 🚀
