# Getting Started with Workflow Improvements

**Quick 5-minute guide to add automated code evaluation and fixes to your CI/CD pipeline.**

## What This Does

Automatically evaluates your code on every push and helps fix issues:
- ✅ Builds firmware (existing)
- ✅ **NEW**: Evaluates code quality
- ✅ **NEW**: Generates fix context for agents/developers
- ✅ **NEW**: Creates PRs or auto-fixes issues

## Choose Your Path

### 🎯 Path 1: Quick Start (Recommended)

**Best for**: Getting started, OTGW-firmware production use

```bash
# Install Option 1 (Multi-Job with PR)
cp docs/workflow-options/option1/main.yml .github/workflows/main.yml
git add .github/workflows/main.yml
git commit -m "feat: Add evaluation and auto-fix workflow"
git push
```

**What happens**: 
- Every push builds, evaluates, and creates a PR if there are failures
- You review and merge the PR
- Safe and controlled

**Next**: See `docs/workflow-options/option1/README.md`

---

### ⚡ Path 2: Fast Iteration

**Best for**: Solo developers, dev branches only

```bash
# Install Option 2 (Inline Fix)
cp docs/workflow-options/option2/main.yml .github/workflows/main.yml
git add .github/workflows/main.yml
git commit -m "feat: Add inline evaluation and auto-fix"
git push
```

**What happens**:
- Every push builds, evaluates, and auto-commits fixes
- No PR overhead
- Fast but less safe

**Next**: See `docs/workflow-options/option2/README.md`

---

### 🎫 Path 3: Enterprise/Governance

**Best for**: Teams needing audit trails and manual control

```bash
# Install Option 3 (Issue-Triggered)
cp docs/workflow-options/option3/main.yml .github/workflows/main.yml
cp docs/workflow-options/option3/agent-fix-trigger.yml .github/workflows/agent-fix-trigger.yml
git add .github/workflows/
git commit -m "feat: Add issue-triggered evaluation and auto-fix"
git push
```

**What happens**:
- Creates issues for failures
- You add a label to trigger agent
- Agent creates PR
- Full audit trail

**Next**: See `docs/workflow-options/option3/README.md`

---

## Not Sure Which to Choose?

Read the detailed comparison: [`docs/workflow-options/COMPARISON.md`](COMPARISON.md)

Or use this quick quiz:

1. **Do you need code review before fixes are applied?**
   - Yes → Option 1 or 3
   - No → Option 2

2. **Do you need to track fix history in issues?**
   - Yes → Option 3
   - No → Option 1

3. **Are you a solo developer on a dev branch?**
   - Yes → Option 2
   - No → Option 1

**Default recommendation for OTGW-firmware: Option 1**

---

## What Each File Does

### Option 1 Files:
- `docs/workflow-options/option1/main.yml` - Complete 3-job workflow
- `docs/workflow-options/option1/README.md` - Installation and usage guide

### Option 2 Files:
- `docs/workflow-options/option2/main.yml` - Single-job workflow
- `docs/workflow-options/option2/README.md` - Installation and customization

### Option 3 Files:
- `docs/workflow-options/option3/main.yml` - Evaluation workflow
- `docs/workflow-options/option3/agent-fix-trigger.yml` - Agent workflow
- `docs/workflow-options/option3/README.md` - Complete setup guide

---

## Testing First (Recommended)

**Always test on a feature branch before deploying to production:**

```bash
# 1. Create test branch
git checkout -b test-ci-improvements

# 2. Install your chosen option (example: Option 1)
cp docs/workflow-options/option1/main.yml .github/workflows/main.yml

# 3. Commit and push to test
git add .github/workflows/main.yml
git commit -m "test: Evaluate workflow option 1"
git push -u origin test-ci-improvements

# 4. Watch workflow run at:
# https://github.com/rvdbreemen/OTGW-firmware/actions

# 5. If it works well, merge to main
git checkout main
git merge test-ci-improvements
git push
```

---

## Verification Checklist

After installation, verify:

- [ ] Workflow appears in GitHub Actions UI
- [ ] Build job still works (existing behavior)
- [ ] Evaluation job runs and uploads report
- [ ] Auto-fix behavior works as expected (PR, commit, or issue)
- [ ] Workflow completes successfully

---

## Troubleshooting

### Workflow Doesn't Run
- Check branch name matches workflow filter
- Verify file is in `.github/workflows/` directory
- Check syntax: `yamllint .github/workflows/main.yml`

### Permissions Error
- Go to repo Settings → Actions → General → Workflow permissions
- Select "Read and write permissions"
- Save

### PR/Issue Not Created
- Check workflow logs in Actions tab
- Verify permissions are enabled (above)
- Check if branch protection is blocking bot

---

## Next Steps After Installation

### Week 1: Monitor
- Watch workflow runs
- Review any PRs/issues created
- Check evaluation reports in artifacts

### Week 2: Refine
- Adjust thresholds if needed
- Customize agent instructions
- Add branch filters

### Week 3: Enhance
- Implement actual fix logic (Option 2)
- Add agent integration (Copilot)
- Auto-merge safe fixes (Optional)

---

## Documentation Index

- **This File**: Quick start guide
- **Executive Summary**: [`EXECUTIVE_SUMMARY.md`](EXECUTIVE_SUMMARY.md)
- **Detailed Comparison**: [`COMPARISON.md`](COMPARISON.md)
- **Main Overview**: [`../WORKFLOW_IMPROVEMENT_OPTIONS.md`](../../WORKFLOW_IMPROVEMENT_OPTIONS.md)
- **Option 1 Details**: [`option1/README.md`](option1/README.md)
- **Option 2 Details**: [`option2/README.md`](option2/README.md)
- **Option 3 Details**: [`option3/README.md`](option3/README.md)

---

## Quick Reference

### Option 1: Multi-Job + PR
```
Push → Build → Eval → PR (if failures)
```
**Files**: `option1/main.yml`
**Time**: 2-5 min
**Review**: Required (PR)

### Option 2: Inline Fix
```
Push → Build → Eval → Auto-commit (if failures)
```
**Files**: `option2/main.yml`
**Time**: 1-2 min
**Review**: None (auto-commits)

### Option 3: Issue-Triggered
```
Push → Build → Eval → Issue → Label → PR
```
**Files**: `option3/main.yml` + `option3/agent-fix-trigger.yml`
**Time**: 3-7 min
**Review**: Two points (issue + PR)

---

## Support

Need help?
1. Check the option-specific README
2. Review troubleshooting section
3. Check GitHub Actions logs
4. Review the comparison guide

---

**Ready? Pick your option above and copy the workflow file(s)!**
