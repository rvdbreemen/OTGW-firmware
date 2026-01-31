# Wiki Sync GitHub Actions - Quick Reference

## Workflow Overview

The `.github/workflows/sync-wiki.yml` GitHub Action automatically synchronizes wiki documentation from the repository to the GitHub wiki.

## How It Works

### Trigger
- **Event**: Push to `dev` or `main` branch
- **Paths**: Changes in `docs/wiki/**` or `.github/workflows/sync-wiki.yml`
- **Runs**: Automatically on push, no manual intervention needed

### Process
1. Checkout repository code
2. Clone the GitHub wiki repository (`.wiki.git`)
3. Copy all `.md` files from `docs/wiki/` to wiki repository
4. Update `_Sidebar.md` with navigation links for v3 documentation
5. Commit changes with detailed message
6. Push to wiki repository
7. Report sync status

### Key Files Modified by Workflow
- All `.md` files copied from `docs/wiki/` → wiki repository
- `_Sidebar.md` - Updated (added v3 API section)
- `_Footer.md` - Preserved (not modified)

## Setup Instructions

### 1. Verify GitHub Token Access
The workflow uses the default `GITHUB_TOKEN` secret, which GitHub automatically provides.

**Check**:
- ✅ Settings → Actions → General → "Read and write permissions"
- ✅ "Allow GitHub Actions to create and approve pull requests" (optional)

### 2. Enable Workflow
1. Go to repository → Actions tab
2. Find "Sync Wiki Documentation"
3. Enable if disabled (usually auto-enabled)

### 3. Test the Workflow

**Option A: Make a test change**
```bash
# Edit a wiki file
git checkout dev
echo "Test update" >> docs/wiki/INTEGRATION_SUMMARY.md
git add docs/wiki/INTEGRATION_SUMMARY.md
git commit -m "test: Test wiki sync workflow"
git push origin dev
```

**Option B: Manual trigger (if available)**
Go to Actions → Sync Wiki Documentation → "Run workflow" button

### 4. Monitor Sync

1. Go to repository → Actions tab
2. Look for "Sync Wiki Documentation" workflow
3. Click the latest run to see details
4. Check wiki at: `https://github.com/rvdbreemen/OTGW-firmware/wiki`

## File Structure

### What Gets Synced
```
docs/wiki/
├── 1-API-v3-Overview.md          ← Synced
├── 2-Quick-Start-Guide.md         ← Synced
├── 3-Complete-API-Reference.md    ← Synced
├── 4-Migration-Guide.md           ← Synced
├── 5-Changelog.md                 ← Synced
├── 6-Error-Handling-Guide.md      ← Synced
├── 7-HATEOAS-Navigation-Guide.md  ← Synced
├── 8-Troubleshooting.md           ← Synced
├── Home.md                         ← Synced
├── All existing wiki pages        ← Synced
└── API-DOCUMENTATION-STRUCTURE.md ← Synced
```

### What the Workflow Updates
```
GitHub Wiki (.wiki repository)
├── All synced .md files from docs/wiki/
└── _Sidebar.md (updated with v3 section)
```

## Workflow Status

### Success Indicators
✅ Green checkmark on commit
✅ "Sync wiki to GitHub Pages" job completed
✅ New commits in wiki repository
✅ Files appear in GitHub wiki within minutes

### Common Messages
- **"No changes detected in wiki"** - No .md files were modified, wiki not updated
- **"Changes detected, proceeding with sync"** - Files synced successfully
- **"Wiki sync completed!"** - Job finished successfully

## Troubleshooting

### Workflow Fails with Permission Error
**Problem**: "Permission denied" when pushing to wiki

**Solution**:
1. Check repo Settings → Actions → General
2. Ensure "Read and write permissions" is enabled
3. Verify GITHUB_TOKEN has wiki access

### Wiki Files Not Updating
**Problem**: Changes pushed but wiki doesn't update

**Check**:
1. Did you push to `dev` or `main` branch?
2. Did you modify files in `docs/wiki/`?
3. Check Actions tab for workflow run status
4. Wiki may take 1-2 minutes to reflect changes

### Workflow Not Running
**Problem**: No workflow run triggered

**Check**:
1. Did you push to `dev` or `main` branch?
2. Changes must be in `docs/wiki/` directory
3. Verify workflow is enabled (Actions tab)
4. Check file paths match the trigger pattern

### Sidebar Not Updating
**Problem**: `_Sidebar.md` not updated with v3 links

**Note**: The workflow only updates `_Sidebar.md` if:
- It doesn't exist yet, OR
- It doesn't already contain "API-v3-Overview" link

**Fix**: If sidebar needs updating, delete it first or manually add v3 section

## Example Workflow Run

```
Event: Push to dev branch
Files changed: docs/wiki/1-API-v3-Overview.md

GitHub Action Triggered:
├─ Checkout repository ✅
├─ Setup git config ✅
├─ Clone wiki repository ✅
├─ Copy wiki files ✅
├─ Create wiki navigation helpers ✅
├─ Check for changes ✅ (Changes detected)
├─ Commit and push wiki changes ✅
│  └─ Commit: "docs: Sync wiki from main repo"
│  └─ Branch: refs/heads/dev
│  └─ Status: pushed to .wiki.git
└─ Report sync status ✅

Wiki Updated:
✅ 1-API-v3-Overview.md
✅ All other .md files
✅ _Sidebar.md (with v3 section)

Total Time: ~30-60 seconds
```

## Maintenance

### Regular Updates
- Edit `.md` files in `docs/wiki/`
- Commit and push to `dev` or `main`
- Workflow automatically syncs

### Adding New Documentation
1. Create `.md` file in `docs/wiki/`
2. Update `Home.md` with link (optional)
3. Push to trigger sync
4. Appears in wiki within minutes

### Removing Documentation
1. Delete `.md` file from `docs/wiki/`
2. Update `Home.md` to remove link
3. Push to trigger sync
4. File removed from wiki within minutes

## Best Practices

✅ **Do**:
- Edit wiki files in `docs/wiki/` folder
- Create pull requests for reviews
- Push to `dev` branch for testing
- Use descriptive commit messages
- Update Home.md when adding new pages

❌ **Don't**:
- Edit wiki directly in GitHub (changes won't sync back)
- Push directly to `main` unless ready for release
- Delete files without updating Home.md
- Break wiki links in Home.md
- Edit `_Sidebar.md` directly (workflow updates it)

## Monitoring Dashboard

GitHub provides a workflow dashboard to monitor syncs:

**Location**: Repository → Actions → "Sync Wiki Documentation"

**Shows**:
- Latest workflow runs
- Run status (success/failure)
- Duration
- Files changed
- Workflow logs

## Related Documentation

- **Workflow file**: `.github/workflows/sync-wiki.yml`
- **Wiki content**: `docs/wiki/` directory
- **Integration guide**: `docs/wiki/API-DOCUMENTATION-STRUCTURE.md`
- **Integration summary**: `docs/wiki/INTEGRATION_SUMMARY.md`
- **Setup guide**: `docs/wiki/WIKI_SETUP.md` (if exists)

## Quick Links

- **GitHub Wiki**: https://github.com/rvdbreemen/OTGW-firmware/wiki
- **GitHub Workflows**: https://github.com/rvdbreemen/OTGW-firmware/actions
- **Repository Settings**: https://github.com/rvdbreemen/OTGW-firmware/settings/actions

## Support

For issues with:
- **Workflow**: Check GitHub Actions logs
- **Wiki content**: See `INTEGRATION_SUMMARY.md`
- **Wiki structure**: See `API-DOCUMENTATION-STRUCTURE.md`
- **GitHub help**: https://docs.github.com/en/actions

