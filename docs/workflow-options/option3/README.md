# Option 3: GitHub Issue-Triggered Agent Workflow

## Overview
This option creates GitHub issues for evaluation failures, then allows manual or automatic triggering of an agent workflow via issue labels. Provides excellent audit trail and control.

## Architecture
```
Push → Build → Evaluate → (if failures) → Create Issue → 
  → Add Label → Trigger Agent Workflow → Create PR
```

## Files Included
- `main.yml` - Build and evaluate workflow with issue creation
- `agent-fix-trigger.yml` - Separate workflow triggered by issue labels

## How to Install

1. **Add both workflow files**:
   ```bash
   cp docs/workflow-options/option3/main.yml .github/workflows/main.yml
   cp docs/workflow-options/option3/agent-fix-trigger.yml .github/workflows/agent-fix-trigger.yml
   ```

2. **Ensure permissions are set** (already in the workflows):
   ```yaml
   permissions:
     contents: write
     pull-requests: write
     issues: write
   ```

3. **Optional: Add issue template**:
   ```bash
   mkdir -p .github/ISSUE_TEMPLATE
   cat > .github/ISSUE_TEMPLATE/auto-fix.md <<'EOF'
   ---
   name: Auto-Fix Request
   about: Request automated fix for code evaluation findings
   labels: auto-fix, evaluation
   ---
   
   ## Evaluation Findings
   
   (Auto-populated by workflow)
   EOF
   ```

4. **Commit and push**:
   ```bash
   git add .github/workflows/
   git commit -m "feat: Add issue-triggered evaluation and auto-fix (Option 3)"
   git push
   ```

## How It Works

### Automatic Flow

1. **On Push**:
   - Build job runs (existing behavior)
   - Evaluate job runs
   - If failures found: Create GitHub issue with evaluation report

2. **Issue Created**:
   - Title: "Code Evaluation Failed: {SHA} ({N} failures)"
   - Labels: `auto-fix`, `evaluation`, `bot`
   - Body contains:
     - Summary of failures
     - Full evaluation report (JSON)
     - Instructions for triggering agent

3. **Manual Trigger**:
   - Developer or automation adds `agent-fix-needed` label to issue

4. **Agent Workflow Triggered**:
   - Parses evaluation report from issue
   - Creates fix branch: `auto-fix/issue-{N}-{timestamp}`
   - Generates agent instructions
   - Creates draft PR with instructions
   - Links PR to issue

5. **Agent or Developer**:
   - Checks out the branch
   - Reads instructions
   - Applies fixes
   - Marks PR ready for review

6. **Review & Merge**:
   - Normal PR review process
   - Issue auto-closes when PR merges

### Manual Flow

You can also manually create issues with the `auto-fix-needed` label to trigger the agent workflow.

## Pros & Cons

### ✅ Advantages
- **Excellent audit trail**: Issues + PRs track all fixes
- **Full control**: Manual trigger via label
- **No noise**: Can review issue before triggering agent
- **History**: Track patterns in evaluation failures over time
- **Flexible**: Can manually fix instead of using agent
- **Safe**: Multiple review points
- **Deduplication**: Avoids creating duplicate issues for same problem

### ❌ Limitations
- **More complex**: Two workflows + issue management
- **Slower**: Extra step (issue creation) adds 1-2 minutes
- **Issue tracker clutter**: Creates issues (can be mitigated)
- **More moving parts**: More to maintain

## Customization

### Auto-Close Old Issues

Add to the issue creation step:
```yaml
# Close old resolved issues
const oldIssues = await github.rest.issues.listForRepo({
  owner: context.repo.owner,
  repo: context.repo.repo,
  labels: 'auto-fix',
  state: 'open'
});

for (const issue of oldIssues.data) {
  if (/* issue is old and resolved */) {
    await github.rest.issues.update({
      owner: context.repo.owner,
      repo: context.repo.repo,
      issue_number: issue.number,
      state: 'closed'
    });
  }
}
```

### Automatic Label Application

To automatically trigger agent (remove manual step):
```yaml
# In main.yml, after creating issue:
await github.rest.issues.addLabels({
  owner: context.repo.owner,
  repo: context.repo.repo,
  issue_number: issue.data.number,
  labels: ['agent-fix-needed']  # Auto-trigger
});
```

**Warning**: This removes human review step!

### Change Issue Deduplication Window

Currently set to 1 hour. To change:
```javascript
const hourAgo = new Date(now - 60 * 60 * 1000);  // Change 60 * 60
```

### Different Labels for Different Fix Types

```yaml
# Low priority fixes
labels: ['auto-fix', 'low-priority']

# High priority fixes  
labels: ['auto-fix', 'critical']
```

Then create separate workflows that respond to different labels.

## Testing Locally

Test components individually:

### Test Issue Creation
```bash
# Run evaluation
python evaluate.py --report

# Simulate issue body creation
node - <<'EOF'
const fs = require('fs');
const report = JSON.parse(fs.readFileSync('evaluation-report.json'));
const failures = report.results.filter(r => r.status === 'FAIL');
console.log(`Would create issue with ${failures.length} failures`);
EOF
```

### Test Agent Instruction Generation
```bash
# Create test issue body
cat > test-issue.md <<'EOF'
```json
{/* paste your evaluation-report.json here */}
```
EOF

# Test parsing
grep -o '```json.*```' test-issue.md | sed 's/```json//;s/```//' | jq .
```

### Full Manual Test
1. Create an issue manually with evaluation report
2. Add `agent-fix-needed` label
3. Watch `agent-fix-trigger.yml` workflow run

## Troubleshooting

### Issues Not Created
- Check workflow logs for "evaluate" job
- Verify `issues: write` permission
- Check if issue deduplication prevented creation

### Too Many Issues
- Reduce the deduplication window (increase from 1 hour)
- Or auto-close old issues
- Or aggregate multiple failures into single issue

### Agent Workflow Not Triggering
- Verify label name is exactly `agent-fix-needed`
- Check `agent-fix-trigger.yml` is in `.github/workflows/`
- Check workflow logs for errors
- Verify permissions are set

### Issue Body Too Large
GitHub has limits on issue body size. If evaluation report is huge:
```yaml
# Upload report as gist instead
const gist = await github.rest.gists.create({
  files: {
    'evaluation-report.json': {
      content: JSON.stringify(report, null, 2)
    }
  },
  public: false
});

// Then link in issue
const issueBody = `Report: ${gist.data.html_url}`;
```

## Issue Management

### Automatically Close Resolved Issues

Add to your PR merge process or create a separate workflow:
```yaml
name: Close Fixed Evaluation Issues

on:
  pull_request:
    types: [closed]

jobs:
  close-issues:
    if: github.event.pull_request.merged == true
    runs-on: ubuntu-latest
    steps:
      - uses: actions/github-script@v7
        with:
          script: |
            // Check if PR was an auto-fix
            if (context.payload.pull_request.labels.some(l => l.name === 'auto-fix')) {
              // Close related issues
              const issueRefs = context.payload.pull_request.body.match(/#(\d+)/g);
              // ... close those issues
            }
```

### Label Management

Suggested labels for this workflow:
- `auto-fix` - Created by automation
- `evaluation` - From evaluation framework
- `bot` - Bot-created issue
- `agent-fix-needed` - Trigger for agent workflow
- `agent-branch-ready` - Agent can now work on the branch
- `critical` / `low-priority` - Priority levels

## Integration with GitHub Copilot Workspace

To integrate with Copilot Workspace:

1. **In `agent-fix-trigger.yml`**, add after creating PR:
   ```yaml
   - name: Trigger Copilot Workspace
     env:
       GH_TOKEN: ${{ secrets.GITHUB_TOKEN }}
     run: |
       # Use GitHub CLI to trigger Copilot Workspace
       gh copilot workspace create \
         --repo ${{ github.repository }} \
         --branch ${{ steps.branch.outputs.branch-name }} \
         --instructions "$(cat agent-instructions.md)"
   ```

2. **Or use GitHub API** if Copilot has an API:
   ```yaml
   - name: Trigger Copilot
     uses: actions/github-script@v7
     with:
       script: |
         // Call Copilot API
         // await github.copilot.workspace.create(...)
   ```

## Recommended Use Cases

This option is best for:
- ✅ Teams with code review requirements
- ✅ Projects requiring audit trails
- ✅ Enterprise environments
- ✅ Projects with governance requirements
- ✅ Safety-critical code (like firmware)
- ✅ When you want control over when fixes are applied

**Not recommended for**:
- ❌ Solo projects (Option 1 or 2 is simpler)
- ❌ High-velocity development (too slow)
- ❌ Projects with minimal process

## Migration Path

Evolve the workflow over time:

1. **Phase 1**: Install as-is (manual trigger only)
2. **Phase 2**: Monitor issue creation, refine templates
3. **Phase 3**: Add auto-labeling for low-risk issues
4. **Phase 4**: Integrate real agent (Copilot, etc.)
5. **Phase 5**: Auto-merge low-risk fixes

## Next Steps

After installing:
1. **Push a change** that triggers evaluation
2. **Review any issues created** - are they useful?
3. **Manually trigger agent** by adding label
4. **Review the PR created** - are instructions clear?
5. **Refine templates** based on results

## Advanced: Multi-Agent Strategy

Use different agents for different categories:

```yaml
# Create separate workflows for each agent
name: Agent Fix - Memory Issues
on:
  issues:
    types: [labeled]

jobs:
  memory-agent:
    if: |
      github.event.label.name == 'agent-fix-memory'
    # ... specialized memory fix agent
```

Then in issue creation, add specific labels based on failure categories.
