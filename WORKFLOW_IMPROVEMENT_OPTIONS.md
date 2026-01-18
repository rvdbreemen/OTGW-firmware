# CI/CD Workflow Improvement Options

**🚀 Quick Start**: Jump straight to [`docs/workflow-options/GETTING_STARTED.md`](docs/workflow-options/GETTING_STARTED.md) for a 5-minute installation guide!

---

## Problem Statement
After a push, we want to:
1. Build the firmware
2. Evaluate the code quality
3. Use evaluation output to kick off an automated agent flow to fix findings

## Current State
- **Build**: Automated via `.github/workflows/main.yml` on push
- **Evaluation**: Manual via `python evaluate.py` 
- **Auto-fix**: None - manual intervention required
- **Evaluation Output**: JSON report with categorized issues (PASS/WARN/FAIL/INFO)

## Three Implementation Options

---

## Option 1: Multi-Job Workflow with Conditional Agent Dispatch

### Architecture
```
Push → Build Job → Evaluate Job → (if failures) → Agent Fix Job → Create PR
```

### How It Works
1. **Build Job**: Existing build process (no changes)
2. **Evaluation Job**: 
   - Runs `python evaluate.py --report --no-color`
   - Uploads evaluation report as artifact
   - Outputs failure count for next job
3. **Agent Fix Job** (conditional):
   - Only runs if evaluation has failures
   - Downloads evaluation report
   - Triggers GitHub Copilot Workspace agent or external agent
   - Agent creates a PR with fixes

### Implementation Files
- `.github/workflows/main.yml` - Enhanced with evaluation and agent jobs
- `.github/actions/evaluate/action.yml` - New composite action for evaluation
- `.github/agents/code-fixer.md` - Agent instructions for fixing evaluation findings
- Optional: `.github/workflows/agent-fix.yml` - Separate reusable workflow

### Pros
✅ Clear separation of concerns (build, evaluate, fix)
✅ Agent only runs when needed (cost-effective)
✅ Can be triggered manually via `workflow_dispatch`
✅ Evaluation report available as artifact for review
✅ Follows existing GitHub Actions patterns in the repo

### Cons
❌ Multiple jobs increase workflow complexity
❌ Requires GitHub token with PR creation permissions
❌ Agent might create conflicting PRs on rapid pushes
❌ More difficult to test locally

### Configuration Required
```yaml
permissions:
  contents: write
  pull-requests: write
  issues: write
```

### Example Workflow Structure
```yaml
jobs:
  build:
    # ... existing build job ...
  
  evaluate:
    needs: build
    runs-on: ubuntu-latest
    outputs:
      has-failures: ${{ steps.check.outputs.has-failures }}
      report-path: ${{ steps.eval.outputs.report-path }}
    steps:
      - uses: actions/checkout@v4
      - name: Run evaluation
        id: eval
        run: python evaluate.py --report --no-color
      - name: Check for failures
        id: check
        run: |
          FAILURES=$(jq '.summary.failed' evaluation-report.json)
          echo "has-failures=$([[ $FAILURES -gt 0 ]] && echo true || echo false)" >> $GITHUB_OUTPUT
      - name: Upload report
        uses: actions/upload-artifact@v4
        with:
          name: evaluation-report
          path: evaluation-report.json
  
  auto-fix:
    needs: evaluate
    if: needs.evaluate.outputs.has-failures == 'true'
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Download evaluation report
        uses: actions/download-artifact@v4
        with:
          name: evaluation-report
      - name: Create fix branch
        run: |
          git checkout -b auto-fix/eval-$(date +%Y%m%d-%H%M%S)
      - name: Trigger agent fix
        # Option A: Use GitHub Copilot API
        # Option B: Use custom agent script
        # Option C: Use issue creation to trigger agent
        run: |
          python scripts/agent-fixer.py --report evaluation-report.json
      - name: Create PR
        uses: peter-evans/create-pull-request@v6
        with:
          title: "Auto-fix: Code evaluation findings"
          body-path: .github/PR_TEMPLATE_AUTOFIX.md
```

### Cost/Complexity
- **Complexity**: Medium (3-4 jobs, conditional logic)
- **Maintenance**: Low (standard GitHub Actions)
- **Agent Cost**: Pay-per-use (only on failures)

---

## Option 2: Single Workflow with In-Process Evaluation and Fix

### Architecture
```
Push → Build → Evaluate → (if failures) → Fix in Same Job → Commit → Push
```

### How It Works
1. **Single Job Workflow**:
   - Build the firmware
   - Run evaluation
   - If failures detected, run agent inline
   - Apply fixes automatically
   - Commit and push changes to same branch
   - Re-run evaluation to verify fixes

### Implementation Files
- `.github/workflows/main.yml` - Enhanced single job
- `scripts/auto-fix-agent.py` - Python script that reads evaluation report and applies fixes
- `.github/agents/inline-fixer.md` - Agent prompt for fixing code

### Pros
✅ Simplest workflow structure (single job)
✅ Immediate fixes on same branch (no PR overhead)
✅ Faster feedback loop
✅ Easy to test locally
✅ No PR management needed
✅ Works well for automated formatting/linting fixes

### Cons
❌ Commits directly to branch (can be disruptive)
❌ No human review before fixes are applied
❌ Risk of infinite loop if fix doesn't work
❌ May trigger additional workflow runs
❌ Not suitable for complex fixes requiring review

### Configuration Required
```yaml
permissions:
  contents: write
```

### Example Workflow Structure
```yaml
jobs:
  build-evaluate-fix:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          token: ${{ secrets.GITHUB_TOKEN }}
      
      - name: Build
        uses: ./.github/actions/build
      
      - name: Evaluate
        id: eval
        run: |
          python evaluate.py --report --no-color
          echo "exit_code=$?" >> $GITHUB_OUTPUT
      
      - name: Auto-fix if needed
        if: steps.eval.outputs.exit_code != '0'
        run: |
          # Run auto-fix script
          python scripts/auto-fix-agent.py --report evaluation-report.json
          
          # Check if any changes were made
          if [[ -n $(git status -s) ]]; then
            git config user.name "github-actions[bot]"
            git config user.email "github-actions[bot]@users.noreply.github.com"
            git add .
            git commit -m "Auto-fix: Applied fixes from code evaluation"
            git push
          fi
      
      - name: Re-evaluate after fixes
        if: steps.eval.outputs.exit_code != '0'
        run: python evaluate.py --report --no-color
      
      - name: Upload final report
        uses: actions/upload-artifact@v4
        with:
          name: evaluation-report
          path: evaluation-report.json
```

### Safety Mechanisms
```python
# scripts/auto-fix-agent.py
MAX_ITERATIONS = 3  # Prevent infinite loops
SAFE_FIXES_ONLY = True  # Only apply low-risk fixes
REQUIRE_VALIDATION = True  # Re-build after each fix
```

### Cost/Complexity
- **Complexity**: Low (single job, simple flow)
- **Maintenance**: Medium (need to maintain auto-fix script)
- **Agent Cost**: Pay-per-use (inline execution)

---

## Option 3: GitHub Issue-Triggered Agent Workflow

### Architecture
```
Push → Build → Evaluate → (if failures) → Create Issue → Issue Triggers Agent → Agent Creates PR
```

### How It Works
1. **Build & Evaluate Workflow**:
   - Build firmware
   - Run evaluation
   - If failures, create GitHub issue with evaluation report
   - Label issue with `auto-fix-needed`
2. **Issue-Triggered Workflow**:
   - Listens for issues with `auto-fix-needed` label
   - Triggers GitHub Copilot Workspace agent
   - Agent analyzes issue body (contains evaluation report)
   - Agent creates PR with fixes
   - Links PR to original issue

### Implementation Files
- `.github/workflows/main.yml` - Build and evaluate, create issue
- `.github/workflows/agent-fix-trigger.yml` - Issue-triggered agent workflow
- `.github/ISSUE_TEMPLATE/auto-fix.md` - Template for auto-generated issues
- `.github/agents/evaluation-fixer.md` - Agent instructions

### Pros
✅ Clean audit trail (issues + PRs)
✅ Human can review issue before agent runs
✅ Can manually trigger agents by labeling issues
✅ GitHub native integration (issues → workflows)
✅ Easy to disable (remove issue creation or label)
✅ Can track history of fixes over time
✅ Supports manual and automated triggers

### Cons
❌ More moving parts (issues + workflows + PRs)
❌ Clutters issue tracker (can be mitigated with auto-close)
❌ Slightly slower (extra issue creation step)
❌ Requires issue management automation

### Configuration Required
```yaml
permissions:
  contents: write
  pull-requests: write
  issues: write
```

### Example Workflow Structure

**File: `.github/workflows/main.yml`**
```yaml
jobs:
  build:
    # ... existing build job ...
  
  evaluate:
    needs: build
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Run evaluation
        id: eval
        run: |
          python evaluate.py --report --no-color
          echo "exit_code=$?" >> $GITHUB_OUTPUT
      
      - name: Create issue for failures
        if: steps.eval.outputs.exit_code != '0'
        uses: actions/github-script@v7
        with:
          script: |
            const fs = require('fs');
            const report = JSON.parse(fs.readFileSync('evaluation-report.json', 'utf8'));
            
            const issueBody = `## Code Evaluation Failed
            
            **Commit**: ${{ github.sha }}
            **Branch**: ${{ github.ref_name }}
            
            ### Summary
            - ✗ Failed: ${report.summary.failed}
            - ⚠ Warnings: ${report.summary.warnings}
            - ✓ Passed: ${report.summary.passed}
            
            ### Failures
            ${report.results.filter(r => r.status === 'FAIL').map(r => 
              \`- **[\${r.category}] \${r.name}**: \${r.message}\`
            ).join('\n')}
            
            <details>
            <summary>Full Report</summary>
            
            \`\`\`json
            ${JSON.stringify(report, null, 2)}
            \`\`\`
            </details>
            
            ---
            
            **Action**: An automated agent will attempt to fix these issues.
            `;
            
            await github.rest.issues.create({
              owner: context.repo.owner,
              repo: context.repo.repo,
              title: \`Auto-fix needed: Code evaluation failed (\${context.sha.substring(0, 7)})\`,
              body: issueBody,
              labels: ['auto-fix-needed', 'bot']
            });
```

**File: `.github/workflows/agent-fix-trigger.yml`**
```yaml
name: Agent Fix Trigger

on:
  issues:
    types: [labeled]

jobs:
  trigger-agent-fix:
    if: github.event.label.name == 'auto-fix-needed'
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Parse evaluation report from issue
        id: parse
        uses: actions/github-script@v7
        with:
          script: |
            const issueBody = context.payload.issue.body;
            const reportMatch = issueBody.match(/```json\n([\s\S]*?)\n```/);
            if (reportMatch) {
              const report = JSON.parse(reportMatch[1]);
              core.setOutput('has-report', 'true');
              require('fs').writeFileSync('evaluation-report.json', reportMatch[1]);
            } else {
              core.setOutput('has-report', 'false');
            }
      
      - name: Create fix branch
        if: steps.parse.outputs.has-report == 'true'
        run: |
          git checkout -b auto-fix/issue-${{ github.event.issue.number }}
      
      - name: Run agent fix
        if: steps.parse.outputs.has-report == 'true'
        run: |
          # Call GitHub Copilot agent or custom agent
          python scripts/agent-fixer.py \
            --report evaluation-report.json \
            --issue ${{ github.event.issue.number }}
      
      - name: Create PR
        if: steps.parse.outputs.has-report == 'true'
        uses: peter-evans/create-pull-request@v6
        with:
          branch: auto-fix/issue-${{ github.event.issue.number }}
          title: "Auto-fix: Resolve evaluation findings (Issue #${{ github.event.issue.number }})"
          body: |
            Automatically generated PR to fix code evaluation findings.
            
            Fixes #${{ github.event.issue.number }}
            
            ## Changes
            - Applied automated fixes based on evaluation report
            - See issue for details
          labels: auto-fix, bot
      
      - name: Comment on issue
        uses: actions/github-script@v7
        with:
          script: |
            await github.rest.issues.createComment({
              owner: context.repo.owner,
              repo: context.repo.repo,
              issue_number: ${{ github.event.issue.number }},
              body: '🤖 Agent fix PR created and linked to this issue.'
            });
```

### Cost/Complexity
- **Complexity**: Medium-High (multiple workflows, issue management)
- **Maintenance**: Medium (workflow + issue templates)
- **Agent Cost**: Pay-per-use (triggered by issues)

---

## Comparison Matrix

| Feature | Option 1: Multi-Job | Option 2: Inline Fix | Option 3: Issue-Triggered |
|---------|-------------------|---------------------|---------------------------|
| **Setup Complexity** | Medium | Low | High |
| **Review Before Fix** | Yes (PR) | No | Yes (Issue + PR) |
| **Audit Trail** | PR only | Commits | Issues + PRs |
| **Response Time** | ~2-5 min | ~1-2 min | ~3-7 min |
| **Cost Control** | Good | Good | Best |
| **Manual Trigger** | Medium | Hard | Easy |
| **Repo Clutter** | Low | None | Medium |
| **Safety** | High | Low | High |
| **Rollback** | Easy (PR) | Hard | Easy (PR) |
| **CI/CD Integration** | Native | Native | Native |
| **Local Testing** | Hard | Easy | Hard |
| **Best For** | Teams, complex fixes | Simple auto-fixes | Enterprise, governance |

---

## Recommendations

### For This Project (OTGW-firmware)
**Recommended: Option 1 (Multi-Job with PR)**

**Reasons:**
1. ✅ ESP8266 firmware is safety-critical (controls heating systems)
2. ✅ Changes should be reviewed before merging
3. ✅ Existing workflow structure supports multiple jobs
4. ✅ Team already uses PR workflow
5. ✅ Evaluation framework is mature and comprehensive
6. ✅ Allows manual review of agent-generated fixes

### Quick Start Path
**Start with Option 1, then enhance:**
1. **Phase 1**: Add evaluation job to workflow (no auto-fix yet)
2. **Phase 2**: Add manual agent trigger via issue label
3. **Phase 3**: Enable automatic PR creation for low-risk fixes
4. **Phase 4**: Integrate GitHub Copilot Workspace agent

### Implementation Priority
1. Add evaluation to CI workflow (all options need this)
2. Create evaluation report artifact (enables all options)
3. Build agent prompt/instructions for fixing common issues
4. Implement chosen option
5. Add safety checks and validation
6. Document for contributors

---

## Implementation Files

Complete, ready-to-use implementations are available in `docs/workflow-options/`:

### Option 1: Multi-Job Workflow
- **Location**: `docs/workflow-options/option1/`
- **Files**: 
  - `main.yml` - Complete 3-job workflow
  - `README.md` - Installation and usage guide
- **Installation**: ~15 minutes
- **Status**: ✅ Production-ready

### Option 2: Inline Fix
- **Location**: `docs/workflow-options/option2/`
- **Files**:
  - `main.yml` - Single-job workflow with inline fixes
  - `README.md` - Installation and customization guide
- **Installation**: ~10 minutes
- **Status**: ✅ Ready (requires fix implementation)

### Option 3: Issue-Triggered
- **Location**: `docs/workflow-options/option3/`
- **Files**:
  - `main.yml` - Build and evaluate workflow
  - `agent-fix-trigger.yml` - Agent workflow
  - `README.md` - Complete setup guide
- **Installation**: ~20 minutes
- **Status**: ✅ Production-ready

## Quick Start

1. **Read the comparison**: See `docs/workflow-options/COMPARISON.md`
2. **Choose an option** based on your needs
3. **Follow the README** in the option's directory
4. **Test** on a feature branch first
5. **Deploy** to your main workflow

## Next Steps

- **Not sure which to choose?** Read [`docs/workflow-options/COMPARISON.md`](docs/workflow-options/COMPARISON.md)
- **Ready to implement?** Navigate to your chosen option's directory
- **Want a hybrid approach?** See the "Hybrid Approach" section in the comparison guide
- **Need help?** Each option has detailed troubleshooting in its README

## Recommendation for OTGW-firmware

**Start with Option 1** (Multi-Job Workflow with PR):
- Safety-critical firmware needs human review
- Provides clear audit trail
- Scales with team growth
- Can add automation later

See `docs/workflow-options/option1/README.md` to get started.
