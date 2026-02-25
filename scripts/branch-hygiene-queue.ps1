param(
    [string]$Remote = "origin",
    [string]$BaseBranch = "dev",
    [int]$InactiveDays = 14,
    [string]$OutputCsv = "docs/process/branch-hygiene-queue.csv"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Test-BranchMergedIntoBase {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourceRemoteBranch,
        [Parameter(Mandatory = $true)]
        [string]$TargetRemoteBranch
    )

    & git merge-base --is-ancestor "refs/remotes/$SourceRemoteBranch" "refs/remotes/$TargetRemoteBranch" 2>$null | Out-Null
    return ($LASTEXITCODE -eq 0)
}

Write-Host "Fetching/pruning remotes..."
& git fetch --all --prune | Out-Null

$baseRemoteBranch = "$Remote/$BaseBranch"
$currentDate = Get-Date

Write-Host "Collecting remote branches from $Remote..."
$rawBranches = & git for-each-ref --sort=-committerdate --format='%(refname:short)|%(committerdate:short)|%(authorname)' "refs/remotes/$Remote"

$excludedBranches = @(
    "$Remote",
    "$Remote/HEAD",
    "$Remote/main",
    "$Remote/dev"
)

$queueRows = @()

foreach ($rawBranch in $rawBranches) {
    if ([string]::IsNullOrWhiteSpace($rawBranch)) {
        continue
    }

    $parts = $rawBranch.Split('|')
    if ($parts.Count -lt 3) {
        continue
    }

    $branchName = $parts[0].Trim()
    $commitDateText = $parts[1].Trim()
    $authorName = $parts[2].Trim()

    if ($excludedBranches -contains $branchName) {
        continue
    }

    $commitDate = [DateTime]::Parse($commitDateText)
    $daysSinceCommit = [Math]::Floor(($currentDate - $commitDate).TotalDays)
    $isMergedIntoBase = Test-BranchMergedIntoBase -SourceRemoteBranch $branchName -TargetRemoteBranch $baseRemoteBranch

    $status = "active"
    $proposedAction = "keep"

    if ($isMergedIntoBase) {
        $status = "stale-merged"
        $proposedAction = "queue-delete"
    }
    elseif ($daysSinceCommit -ge $InactiveDays) {
        $status = "stale-unmerged"
        $proposedAction = "owner-review"
    }

    $queueRows += [PSCustomObject]@{
        Branch          = $branchName
        LastCommitDate  = $commitDate.ToString("yyyy-MM-dd")
        DaysSinceCommit = [int]$daysSinceCommit
        Author          = $authorName
        MergedIntoBase  = if ($isMergedIntoBase) { "yes" } else { "no" }
        Status          = $status
        ProposedAction  = $proposedAction
        Owner           = ""
        Decision        = ""
        Notes           = ""
    }
}

$outputDirectory = Split-Path -Path $OutputCsv -Parent
if (-not [string]::IsNullOrWhiteSpace($outputDirectory)) {
    New-Item -Path $outputDirectory -ItemType Directory -Force | Out-Null
}

$queueRows |
    Sort-Object -Property @{ Expression = 'Status'; Descending = $false }, @{ Expression = 'DaysSinceCommit'; Descending = $true } |
    Export-Csv -Path $OutputCsv -NoTypeInformation -Encoding UTF8

$summary = $queueRows | Group-Object -Property Status | Sort-Object -Property Name
Write-Host "Branch queue written to: $OutputCsv"
Write-Host "Summary:"
foreach ($group in $summary) {
    Write-Host "  $($group.Name): $($group.Count)"
}
