# Option 1: Multi-Job Workflow with Conditional Agent Dispatch

## Overview
This option adds evaluation as a separate job after build, then conditionally triggers an auto-fix job that creates a PR with context for an agent to fix the issues.

## Architecture
```
Push → Build Job → Evaluate Job → (if failures) → Auto-Fix Job → Create PR
```

## Files Included
- `main.yml` - Enhanced CI workflow with 3 jobs: build, evaluate, auto-fix

## How to Install

1. **Replace your `.github/workflows/main.yml`**:
   ```bash
   cp docs/workflow-options/option1/main.yml .github/workflows/main.yml
   ```

2. **Ensure permissions are set** (already in the workflow):
   ```yaml
   permissions:
     contents: write
     pull-requests: write
     issues: read
   ```

3. **Commit and push**:
   ```bash
   git add .github/workflows/main.yml
   git commit -m "feat: Add evaluation and auto-fix to CI workflow (Option 1)"
   git push
   ```

## How It Works

### On Every Push
1. **Build Job**: Builds firmware (existing behavior)
2. **Evaluate Job**: 
   - Runs `python evaluate.py --report --no-color`
   - Parses results and uploads evaluation report as artifact
   - Outputs whether there are failures
3. **Auto-Fix Job** (conditional):
   - Only runs if evaluation has failures
   - Creates a new branch: `auto-fix/eval-YYYYMMDD-HHMMSS-SHA`
   - Generates agent instructions from evaluation report
   - Creates a PR with:
     - Evaluation report embedded
     - Agent instructions for fixing
     - Links to artifacts
   - PR is in ready-for-review state (not draft)

### Review Process
1. Developer or agent reviews the PR
2. Agent can be triggered by adding `agent-fix-needed` label (optional enhancement)
3. Once fixed, PR is merged normally

## Pros & Cons

### ✅ Advantages
- Clear separation of build, evaluate, and fix stages
- Human review before any changes are applied
- Evaluation report always available as artifact
- Agent only runs when needed (cost-effective)
- Can manually trigger workflow
- Follows GitHub PR best practices

### ❌ Limitations
- Multiple jobs add some complexity
- Requires PR management
- Slightly slower than inline fixes (2-5 minutes total)
- May create multiple PRs if multiple pushes fail evaluation

## Customization

### Adjust What Triggers Auto-Fix
Edit the condition in `main.yml`:
```yaml
auto-fix:
  needs: evaluate
  if: needs.evaluate.outputs.has-failures == 'true'  # Change this
```

### Change Branch Naming
```yaml
BRANCH_NAME="auto-fix/eval-$(date +%Y%m%d-%H%M%S)-${{ github.sha }}"
```

### Add Agent Integration
To integrate with GitHub Copilot or other agents, modify the "Run agent fix" step:
```yaml
- name: Trigger agent fix
  run: |
    # Call your agent API here
    # Example: gh copilot workspace create --instructions agent-prompt.md
```

## Testing Locally

You can't fully test GitHub Actions locally, but you can test the components:

```bash
# Test evaluation
python evaluate.py --report

# Test fix instruction generation
python - <<'EOF'
import json
with open('evaluation-report.json') as f:
    report = json.load(f)
failures = [r for r in report['results'] if r['status'] == 'FAIL']
print(f"Would create PR for {len(failures)} failures")
EOF
```

## Troubleshooting

### PR Not Created
- Check that failures were detected: View workflow logs for "evaluate" job
- Check permissions: Ensure `pull-requests: write` is set
- Check branch protection: May prevent bot from creating PRs

### Too Many PRs
- Add deduplication logic to check for existing open auto-fix PRs
- Or use Option 3 (issue-triggered) which has built-in deduplication

### Agent Instructions Not Clear
- Edit the "Create agent prompt" step in `main.yml`
- Add more context specific to your common issues

## Next Steps

After installing:
1. Push a change that triggers the workflow
2. Watch the workflow run in GitHub Actions
3. Review any PRs created by the auto-fix job
4. Refine agent instructions based on results

## Migration Path

This option can evolve:
- **Phase 1**: Install as-is (creates PRs for review)
- **Phase 2**: Add actual agent integration (Copilot, etc.)
- **Phase 3**: Auto-merge low-risk fixes (formatting, etc.)
