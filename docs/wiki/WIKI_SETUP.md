# Wiki Automation Setup Guide

This document explains how the wiki automation works and how to enable it.

## Overview

The wiki documentation is automatically synced from the repository's `docs/wiki/` folder to GitHub Pages using GitHub Actions.

### Architecture

```
docs/wiki/*.md (Source)
        ↓
  GitHub Actions
        ↓
  <repo>.wiki.git (Target)
        ↓
GitHub Pages Wiki
```

**Benefits:**
- ✅ Single source of truth (in main repository)
- ✅ Version control (git history for all changes)
- ✅ PR workflow (review before publishing)
- ✅ Automatic sync (no manual updates needed)
- ✅ Atomic updates (all-or-nothing)

## Setup Instructions

### 1. Enable GitHub Actions (if not already enabled)

Go to repository settings:
1. Navigate to **Settings** → **Actions**
2. Select **"Allow all actions and reusable workflows"**
3. Click **Save**

### 2. Ensure Wiki is Enabled

1. Go to **Settings** → **Features**
2. Check the **"Wikis"** checkbox
3. Click **Save**

### 3. Grant GITHUB_TOKEN Wiki Permissions

GitHub Actions workflows use the `GITHUB_TOKEN` secret for authentication.

**Default permissions** should work, but verify:

1. Go to **Settings** → **Code and automation** → **Actions** → **General**
2. Under "Workflow permissions", select **"Read and write permissions"**
3. Check **"Allow GitHub Actions to create and approve pull requests"** if desired
4. Click **Save**

### 4. Initial Wiki Setup (One-time)

The workflow will create the wiki repository automatically on first run.

To trigger the first sync:

```bash
# Make a change to docs/wiki/ and push
echo "# Test" >> docs/wiki/test.md
git add docs/wiki/test.md
git commit -m "test: trigger wiki sync"
git push origin dev
```

GitHub Actions will automatically:
1. Clone the wiki repository
2. Copy wiki files
3. Create navigation pages (Home.md, _Sidebar.md, _Footer.md)
4. Push changes to wiki

**Wait 2-3 minutes for the workflow to complete.**

### 5. Verify Setup

Check that the workflow completed successfully:

1. Go to **Actions** tab
2. Click **"Sync Wiki Documentation"** workflow
3. Check latest run (should show ✅ green checkmark)
4. Navigate to **Wiki** tab to view published content

## How It Works

### Workflow Trigger

The workflow runs automatically when:
- Files in `docs/wiki/` are changed
- Workflow file itself is modified
- Push to `dev` or `main` branch

### Workflow Steps

1. **Checkout Repository**
   - Clones current repository with full history

2. **Setup Git Config**
   - Configures git user for commits (github-actions bot)

3. **Clone Wiki Repository**
   - Clones `<repo>.wiki.git` repository
   - Uses `GITHUB_TOKEN` for authentication

4. **Copy Wiki Files**
   - Removes old wiki files
   - Copies new files from `docs/wiki/`

5. **Create Navigation Pages**
   - Generates `Home.md` (wiki homepage)
   - Generates `_Sidebar.md` (navigation menu)
   - Generates `_Footer.md` (footer on all pages)

6. **Commit and Push**
   - Stages all changes
   - Creates commit with branch/commit info
   - Pushes to wiki repository

## File Organization

### In Main Repository

```
docs/wiki/
├── 1-API-v3-Overview.md
├── 2-Quick-Start-Guide.md
├── 3-Complete-API-Reference.md
├── 4-Migration-Guide.md
├── 5-Changelog.md
├── 6-Error-Handling-Guide.md
├── 7-HATEOAS-Navigation-Guide.md
└── 8-Troubleshooting.md
```

### In Wiki Repository (auto-generated)

```
.wiki/
├── Home.md (auto-generated index)
├── _Sidebar.md (auto-generated navigation)
├── _Footer.md (auto-generated footer)
├── 1-API-v3-Overview.md
├── 2-Quick-Start-Guide.md
├── 3-Complete-API-Reference.md
├── 4-Migration-Guide.md
├── 5-Changelog.md
├── 6-Error-Handling-Guide.md
├── 7-HATEOAS-Navigation-Guide.md
└── 8-Troubleshooting.md
```

## Creating Wiki Content

### 1. Create Markdown File

Add new wiki content to `docs/wiki/`:

```bash
# Create new wiki page
echo "# My New Topic\n\nContent here..." > docs/wiki/9-My-Topic.md

# Or edit existing page
vim docs/wiki/1-API-v3-Overview.md
```

### 2. Commit and Push

```bash
# Stage changes
git add docs/wiki/9-My-Topic.md

# Commit
git commit -m "docs: Add new wiki topic"

# Push
git push origin dev
```

### 3. Workflow Auto-Syncs

GitHub Actions will:
1. Detect changes to `docs/wiki/` ✅
2. Run the sync workflow ⚙️
3. Update the wiki with new content 📖
4. Appear in GitHub Pages wiki (2-3 minutes)

## Updating Navigation

The sidebar is **auto-generated** from the workflow.

To customize navigation, edit the workflow file:

```
.github/workflows/sync-wiki.yml
```

Find the section:
```yaml
- name: Create wiki index
  run: |
    cd wiki-repo
    cat > _Sidebar.md << 'EOF'
    # Edit navigation here
    EOF
```

## Troubleshooting

### Workflow Won't Run

**Check:**
1. Workflow file exists: `.github/workflows/sync-wiki.yml`
2. Changes are in `docs/wiki/` folder
3. Push is to `dev` or `main` branch
4. Actions are enabled in repository settings

### Wiki Repository Not Found

**Solution:**
1. Create empty wiki by going to **Wiki** tab
2. Click **"Create the first page"**
3. Create any page (can delete later)
4. This initializes the wiki repository
5. Re-run the workflow

### Changes Not Showing in Wiki

**Check:**
1. Workflow completed successfully (✅ in Actions tab)
2. Check latest workflow run for errors
3. Wait 2-3 minutes for GitHub Pages to rebuild
4. Hard refresh wiki page (Ctrl+Shift+R)
5. Check wiki repository directly:
   ```
   https://github.com/<owner>/<repo>.wiki.git
   ```

### Merge Conflicts

If manual wiki edits conflict with auto-sync:
1. Wiki repository is overwritten (manual edits lost)
2. Always edit in `docs/wiki/` folder
3. Don't edit wiki files directly on GitHub

## Manual Sync (if needed)

You can manually run the workflow:

1. Go to **Actions** tab
2. Click **"Sync Wiki Documentation"**
3. Click **"Run workflow"**
4. Select branch: `dev` or `main`
5. Click **"Run workflow"**

## Best Practices

### ✅ Do This

```bash
# Always edit in docs/wiki/
vim docs/wiki/1-API-v3-Overview.md

# Commit meaningful messages
git commit -m "docs: Update API overview with examples"

# Push to main repository
git push origin dev
```

### ❌ Don't Do This

```bash
# Don't edit wiki files directly on GitHub
# (changes will be overwritten on next sync)

# Don't edit in .wiki.git repository
# (edits will be lost)

# Don't manually sync without updating source
# (changes won't persist)
```

## Performance

- **Sync time**: ~30-60 seconds
- **First appearance in wiki**: 2-3 minutes
- **Rate limiting**: None (syncs on every change)
- **Storage**: ~5MB for 8 wiki pages

## Security

- **Authentication**: Uses `GITHUB_TOKEN` (scoped to this repository)
- **Permissions**: Read and write to wiki repository only
- **Commit author**: github-actions bot
- **No credentials stored**: Uses GITHUB_TOKEN provided by GitHub Actions

## Related Files

- **Workflow**: `.github/workflows/sync-wiki.yml`
- **Wiki Source**: `docs/wiki/`
- **README**: `docs/wiki/README.md` (optional, internal documentation)

## FAQ

**Q: Can I edit the wiki directly on GitHub?**  
A: Not recommended. Changes will be overwritten on next sync. Always edit `docs/wiki/` instead.

**Q: How often does the sync run?**  
A: Every time files in `docs/wiki/` change on `dev` or `main` branch.

**Q: Can I include images in wiki pages?**  
A: Yes! Use markdown image syntax. Store images in `docs/wiki/images/` or use external URLs.

**Q: What if the workflow fails?**  
A: Check the Actions tab for error messages. Common causes: wiki repo not initialized, GITHUB_TOKEN permissions missing.

**Q: Can I schedule periodic syncs?**  
A: Currently set to sync on push only. To add scheduled syncs, edit the `on:` section in workflow YAML.

## Monitoring

Check workflow status:

```bash
# View workflow runs
gh run list -w "Sync Wiki Documentation"

# View latest run
gh run view -w "Sync Wiki Documentation"

# View logs of latest run
gh run view -w "Sync Wiki Documentation" --log
```

(Requires GitHub CLI: https://cli.github.com/)

## Next Steps

1. ✅ Enable Actions and Wiki in repository settings
2. ✅ Grant GITHUB_TOKEN wiki permissions
3. ✅ Edit `docs/wiki/` files as needed
4. ✅ Push changes to `dev` or `main`
5. ✅ Monitor workflow in Actions tab
6. ✅ View published content in Wiki tab

## Support

- 🐛 **Workflow Issues**: [GitHub Issues](https://github.com/rvdbreemen/OTGW-firmware/issues)
- 💬 **Questions**: [GitHub Discussions](https://github.com/rvdbreemen/OTGW-firmware/discussions)
- 📖 **GitHub Actions Docs**: https://docs.github.com/en/actions

---

**Wiki automation is now set up!** 🎉 Create or edit files in `docs/wiki/` and they'll automatically sync to the GitHub Pages wiki.
