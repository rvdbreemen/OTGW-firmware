<#
.SYNOPSIS
  Applies the OTGW design system package into a local OTGW-firmware checkout.

.DESCRIPTION
  Backs up data/ as a timestamped zip, then copies the design system files
  from this package's data/ folder into <RepoPath>\src\OTGW-firmware\data\.

  Default mode is INTERACTIVE  -  you confirm each file replacement.
  Pass -Force to skip prompts.

  This script does NOT:
    - Edit existing files (no find/replace, no markup rewrite)
    - Run builds or flashes
    - Touch git
  Those are intentional Claude Code's job  -  see handoff.md.

.PARAMETER RepoPath
  Root of your OTGW-firmware checkout (the folder that contains
  src\OTGW-firmware\data). Required.

.PARAMETER Force
  Skip per-file confirmation prompts. Backup is still created.

.PARAMETER NoBackup
  Skip the backup step (NOT recommended  -  only for re-runs).

.PARAMETER WhatIf
  Show what would happen without changing anything.

.EXAMPLE
  .\apply.ps1 -RepoPath C:\dev\OTGW-firmware

.EXAMPLE
  .\apply.ps1 -RepoPath C:\dev\OTGW-firmware -Force

.EXAMPLE
  .\apply.ps1 -RepoPath C:\dev\OTGW-firmware -WhatIf

.NOTES
  Author : OTGW design package
  Tested : Windows 10 / 11, PowerShell 5.1 + 7.x
#>

[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$RepoPath,

    [switch]$Force,
    [switch]$NoBackup
)

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------- helpers

function Write-Step    { param($m) Write-Host ""; Write-Host "==> $m" -ForegroundColor Cyan }
function Write-Ok      { param($m) Write-Host "    OK   $m" -ForegroundColor Green }
function Write-Skip    { param($m) Write-Host "    skip $m" -ForegroundColor DarkGray }
function Write-WarnMsg { param($m) Write-Host "    WARN $m" -ForegroundColor Yellow }
function Write-Err     { param($m) Write-Host "    FAIL $m" -ForegroundColor Red }

function Confirm-Step {
    param([string]$Prompt)
    if ($Force) { return $true }
    while ($true) {
        $a = Read-Host "$Prompt [Y/n/q]"
        if ([string]::IsNullOrWhiteSpace($a) -or $a -match '^[Yy]') { return $true }
        if ($a -match '^[Nn]') { return $false }
        if ($a -match '^[Qq]') { Write-Host ""; Write-WarnMsg "Aborted by user."; exit 2 }
    }
}

# ---------------------------------------------------------------- banner

Write-Host ""
Write-Host "  OTGW Design System  -  apply.ps1" -ForegroundColor White
Write-Host "  ------------------------------" -ForegroundColor White
Write-Host ""

# ---------------------------------------------------------------- resolve paths

$here       = Split-Path -Parent $MyInvocation.MyCommand.Path
$pkgData    = Join-Path $here 'data'

if (-not (Test-Path $pkgData)) {
    Write-Err "Package's data/ folder not found at: $pkgData"
    Write-Err "Run this script from inside the otgw_design_package folder."
    exit 1
}

if (-not (Test-Path $RepoPath)) {
    Write-Err "RepoPath does not exist: $RepoPath"
    exit 1
}

$targetData = Join-Path $RepoPath 'src\OTGW-firmware\data'
if (-not (Test-Path $targetData)) {
    Write-WarnMsg "Standard path src\OTGW-firmware\data not found under $RepoPath."
    Write-WarnMsg "Trying $RepoPath\data ..."
    $targetData = Join-Path $RepoPath 'data'
    if (-not (Test-Path $targetData)) {
        Write-Err "Could not locate firmware data/ folder. Aborting."
        Write-Err "Expected one of:"
        Write-Err "  $($RepoPath)\src\OTGW-firmware\data"
        Write-Err "  $($RepoPath)\data"
        exit 1
    }
}

Write-Host "  Package source : $pkgData"
Write-Host "  Firmware data/ : $targetData"
Write-Host "  Mode           : $(if ($Force) {'Force (no prompts)'} else {'Interactive'})"
Write-Host ""

# ---------------------------------------------------------------- backup

if (-not $NoBackup) {
    Write-Step "Step 1  -  backup data/"

    $stamp     = Get-Date -Format 'yyyyMMdd-HHmmss'
    $backupDir = Join-Path $here 'backups'
    if (-not (Test-Path $backupDir)) {
        New-Item -Path $backupDir -ItemType Directory | Out-Null
    }
    $backupZip = Join-Path $backupDir "data-backup-$stamp.zip"

    if ($PSCmdlet.ShouldProcess($targetData, "Compress-Archive -> $backupZip")) {
        try {
            Compress-Archive -Path "$targetData\*" -DestinationPath $backupZip -CompressionLevel Optimal -Force
            $size = (Get-Item $backupZip).Length
            Write-Ok ("Backup -> {0} ({1:N0} bytes)" -f $backupZip, $size)
        } catch {
            Write-Err "Backup failed: $($_.Exception.Message)"
            exit 1
        }
    }
} else {
    Write-WarnMsg "Step 1  -  backup SKIPPED (-NoBackup)"
}

# ---------------------------------------------------------------- copy plan

Write-Step "Step 2  -  copy plan"

# Files in the package that will land in the firmware data/ folder.
# Path is relative to package data/ AND to firmware data/.
$plan = @(
    'ds-tokens.css',
    'components.css',
    'theme-toggle.js',
    'sat-slider.js',
    'echarts-theme.js',
    'design.html',
    'fonts\inter-400.woff2',
    'fonts\inter-700.woff2',
    'fonts\jetbrains-mono-400.woff2'
)

# Pretty diff table
$rows = @()
foreach ($rel in $plan) {
    $src = Join-Path $pkgData $rel
    $dst = Join-Path $targetData $rel
    if (-not (Test-Path $src)) {
        Write-Err "Missing in package: $rel"
        exit 1
    }
    $exists = Test-Path $dst
    if ($exists) { $fileStatus = 'OVERWRITE' } else { $fileStatus = 'NEW' }
    $rows += [pscustomobject]@{
        File   = $rel
        Status = $fileStatus
        Size   = (Get-Item $src).Length
    }
}

$rows | Format-Table -AutoSize | Out-String | Write-Host

if (-not $Force) {
    if (-not (Confirm-Step "Proceed with the copy plan above?")) {
        Write-WarnMsg "Aborted before copy."
        exit 2
    }
}

# ---------------------------------------------------------------- copy

Write-Step "Step 3  -  copy files"

$copied  = 0
$skipped = 0

foreach ($rel in $plan) {
    $src = Join-Path $pkgData    $rel
    $dst = Join-Path $targetData $rel

    # Per-file prompt in interactive mode (Force already handled above)
    $proceed = $true
    if (-not $Force -and (Test-Path $dst)) {
        $proceed = Confirm-Step "  Overwrite $rel ?"
    }

    if (-not $proceed) {
        Write-Skip $rel
        $skipped++
        continue
    }

    if ($PSCmdlet.ShouldProcess($dst, "Copy from $src")) {
        $dstDir = Split-Path -Parent $dst
        if (-not (Test-Path $dstDir)) {
            New-Item -Path $dstDir -ItemType Directory | Out-Null
        }
        Copy-Item -Path $src -Destination $dst -Force
        Write-Ok $rel
        $copied++
    }
}

# ---------------------------------------------------------------- summary

Write-Step "Done"

Write-Host "  Copied   : $copied"
Write-Host "  Skipped  : $skipped"
Write-Host ""
Write-Host "  Next steps (these are NOT automated):" -ForegroundColor White
Write-Host "    1. Open the templates in <package>\templates\ and apply the marked"
Write-Host "       blocks to data\index.html, sat.html, settings.html, log.html,"
Write-Host "       FSexplorer.html (paste between DS:…:BEGIN / DS:…:END sentinels)."
Write-Host "    2. Make the two small sat.js edits documented in sat.html.template."
Write-Host "    3. Hand off to Claude Code with handoff.md for code review and"
Write-Host "       per-patch commits."
Write-Host "    4. Build + flash LittleFS (PlatformIO, your existing flow)."
Write-Host ""
Write-Host "  Backup zip: $backupZip" -ForegroundColor DarkGray
Write-Host ""
