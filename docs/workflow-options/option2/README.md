# Option 2: Single Workflow with Inline Evaluation and Fix

## Overview
This option combines build, evaluation, and automated fixes in a single job. Fixes are applied directly to the branch without creating a PR.

## Architecture
```
Push → Build → Evaluate → (if failures) → Fix → Commit → Push
```

## Files Included
- `main.yml` - Single job workflow that builds, evaluates, and fixes inline

## How to Install

1. **Replace your `.github/workflows/main.yml`**:
   ```bash
   cp docs/workflow-options/option2/main.yml .github/workflows/main.yml
   ```

2. **Ensure permissions are set** (already in the workflow):
   ```yaml
   permissions:
     contents: write
   ```

3. **Commit and push**:
   ```bash
   git add .github/workflows/main.yml
   git commit -m "feat: Add inline evaluation and auto-fix to CI (Option 2)"
   git push
   ```

## How It Works

### On Every Push
1. **Build**: Builds firmware using existing build action
2. **Evaluate**: Runs code evaluation
3. **Auto-Fix** (if needed):
   - Applies automated fixes using Python script
   - Only fixes safe, mechanical issues
   - Commits changes back to the same branch
   - Re-runs evaluation to verify
4. **Re-Evaluate**: Checks if fixes resolved the issues
5. **Commit & Push**: Automatically commits fixes to the branch

### What Gets Auto-Fixed
The included auto-fix script is a framework. By default, it only logs what it would fix:
- Missing PROGMEM usage
- Serial.print usage (should be Debug macros)
- Other safe, mechanical fixes

You need to implement the actual fix logic based on your needs.

## Pros & Cons

### ✅ Advantages
- Simplest workflow (single job)
- Immediate fixes (no PR overhead)
- Fast feedback (1-2 minutes)
- Easy to test locally
- No PR management needed
- Great for formatting and linting fixes

### ❌ Limitations
- Commits directly to branch (can be disruptive)
- No human review before fixes
- Risk of triggering multiple workflow runs
- May cause merge conflicts if multiple people pushing
- Not suitable for complex fixes requiring review
- **IMPORTANT**: Not recommended for main/production branches

## Safety Mechanisms

The workflow includes several safety features:

1. **Build Verification**: Re-builds after applying fixes
2. **Re-Evaluation**: Verifies fixes actually work
3. **Limited Scope**: Only applies safe, known fixes
4. **Detailed Commit Messages**: Shows what was fixed

### Preventing Infinite Loops

The auto-fix script should have limits:
```python
MAX_ITERATIONS = 3  # Don't try forever
SAFE_FIXES_ONLY = True  # Only mechanical fixes
```

## Customization

### Implement Actual Fixes

Edit the Python script in the workflow to implement real fixes:

```yaml
- name: Apply automated fixes
  run: |
    cat > /tmp/auto-fix.py <<'EOFIX'
    # ... implement your fixes here
    EOFIX
```

Example fix implementations:
```python
def fix_progmem_usage(self, finding):
    """Fix missing PROGMEM in string literals"""
    # Parse finding details to get file and line
    # Apply fix using regex or AST manipulation
    # Example:
    pattern = r'const char (\w+)\[\] = "(.*?)";'
    replacement = r'const char \1[] PROGMEM = "\2";'
    # Apply to files...
```

### Restrict to Specific Branches

```yaml
on:
  push:
    branches:
      - dev          # Only on dev branch
      - 'dev-*'      # And dev feature branches
      # Remove 'main' to prevent auto-fixes on production
```

### Add Manual Approval

Change to manual workflow dispatch:
```yaml
on:
  workflow_dispatch:
    inputs:
      apply_fixes:
        description: 'Apply fixes automatically'
        required: true
        type: boolean
```

## Testing Locally

This option is easy to test locally:

```bash
# 1. Run evaluation
python evaluate.py --report

# 2. Test auto-fix script
cat > /tmp/auto-fix.py <<'EOF'
# Copy the script from the workflow
EOF

python /tmp/auto-fix.py

# 3. Verify build still works
make -j$(nproc)

# 4. Re-evaluate
python evaluate.py --quick
```

## Troubleshooting

### Infinite Loop (Workflow Keeps Triggering)
- **Cause**: Fix commits trigger new workflow runs
- **Solution**: Add `[skip ci]` to commit message:
  ```yaml
  git commit -m "Auto-fix: Apply fixes [skip ci]"
  ```

### Fixes Not Applied
- Check the auto-fix script output in workflow logs
- Verify fixes are actually implemented (not just logged)
- Check file permissions

### Build Fails After Fix
- Auto-fix broke something
- Roll back the commit:
  ```bash
  git revert HEAD
  git push
  ```

### Merge Conflicts
- If multiple developers push simultaneously, conflicts may occur
- Use Option 1 (PR-based) instead for multi-developer teams

## Recommended Use Cases

This option is best for:
- ✅ Solo developer projects
- ✅ Development branches only
- ✅ Simple, mechanical fixes (formatting, imports, etc.)
- ✅ Quick iteration cycles
- ✅ CI/CD pipelines with auto-formatting

**Not recommended for**:
- ❌ Production/main branches
- ❌ Multi-developer teams (use Option 1)
- ❌ Complex fixes requiring review
- ❌ Safety-critical code (like firmware)

## Migration Path

Start safe, expand gradually:

1. **Phase 1**: Enable only on dev branches, log-only mode
2. **Phase 2**: Implement one safe fix type (e.g., formatting)
3. **Phase 3**: Test thoroughly, add more fix types
4. **Phase 4**: Expand to more branches if successful

## Converting to PR-Based (Option 1)

If you find inline fixes too aggressive, you can convert to Option 1:
1. Keep the evaluation job
2. Remove the commit/push steps
3. Add PR creation instead
4. See Option 1 README for details

## Next Steps

After installing:
1. **Test on a feature branch first**: Don't enable on main immediately
2. **Implement one fix type**: Start with something simple
3. **Monitor for issues**: Check workflow logs and commits
4. **Refine over time**: Add more fix types as confidence grows

## Example: Implementing a PROGMEM Fix

Here's a real example you could add to the auto-fix script:

```python
def fix_missing_f_macro(self):
    """Fix string literals in Debug calls that should use F()"""
    import re
    
    for ino_file in self.project_dir.glob('*.ino'):
        content = ino_file.read_text()
        original = content
        
        # Fix DebugTln("literal") -> DebugTln(F("literal"))
        content = re.sub(
            r'DebugTln\("([^"]+)"\)',
            r'DebugTln(F("\1"))',
            content
        )
        
        # Fix DebugTf("literal", ...) -> DebugTf(PSTR("literal"), ...)
        content = re.sub(
            r'DebugTf\("([^"]+)"',
            r'DebugTf(PSTR("\1")',
            content
        )
        
        if content != original:
            ino_file.write_text(content)
            self.changes_made += 1
            self.fixes_applied.append(f"F() macro: {ino_file.name}")
```

Add this method to the `AutoFixer` class and call it from `apply_fixes()`.
