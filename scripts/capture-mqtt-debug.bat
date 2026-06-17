@echo off
rem ============================================================================
rem  capture-mqtt-debug.bat  -  OTGW MQTT + telnet diagnostic capture (Windows)
rem
rem  SINGLE-FILE DELIVERY: this .bat is the only file you need. The PowerShell
rem  worker is embedded at the bottom of this file (after the PSPAYLOAD marker
rem  line) and extracted to a temporary .ps1 at run time. PowerShell does all
rem  the actual work; the .bat is just a low-friction launcher you can
rem  double-click or run from cmd. No separate .ps1 to ship, no execution-policy
rem  prompt (Bypass is set for the child process only).
rem
rem  Usage:
rem    capture-mqtt-debug.bat                 (interactive prompts)
rem    capture-mqtt-debug.bat --help
rem    capture-mqtt-debug.bat -DeviceHost 192.168.1.50 -BrokerHost 192.168.1.10
rem  All arguments are forwarded verbatim to the embedded PowerShell script.
rem ============================================================================
setlocal EnableExtensions DisableDelayedExpansion

set "SELF=%~f0"

rem --- locate PowerShell (prefer pwsh 7+, fall back to Windows PowerShell) ---
set "PS_EXE="
where pwsh >nul 2>&1 && set "PS_EXE=pwsh"
if not defined PS_EXE ( where powershell >nul 2>&1 && set "PS_EXE=powershell" )
if not defined PS_EXE (
    echo PowerShell was not found on PATH.>&2
    exit /b 1
)

rem --- map cmd-style help flags to the script's -Help switch ---
set "FWD=%*"
if /I "%~1"=="--help" set "FWD=-Help"
if /I "%~1"=="/?"     set "FWD=-Help"

rem --- extract the embedded PowerShell payload to a temp file (verbatim) ---
set "TEMP_STAMP=%TIME::=%"
set "TEMP_STAMP=%TEMP_STAMP:.=%"
set "TEMP_STAMP=%TEMP_STAMP: =0%"
set "PS1=%TEMP%\capture-mqtt-debug-%TEMP_STAMP%-%RANDOM%%RANDOM%%RANDOM%.ps1"
"%PS_EXE%" -NoProfile -ExecutionPolicy Bypass -Command "$src = Get-Content -LiteralPath $env:SELF; $i = ($src | Select-String -Pattern '^:::PSPAYLOAD:::' | Select-Object -First 1).LineNumber; Set-Content -LiteralPath $env:PS1 -Value ($src[$i..($src.Count-1)]) -Encoding UTF8"
if not exist "%PS1%" (
    echo Failed to extract embedded PowerShell payload.>&2
    exit /b 1
)

rem --- run the worker in this console; /b keeps cmd from owning Ctrl+C handling ---
start "" /b /wait "%PS_EXE%" -NoProfile -ExecutionPolicy Bypass -File "%PS1%" %FWD%
set "RC=%ERRORLEVEL%"

del "%PS1%" >nul 2>&1
endlocal & exit /b %RC%

:::PSPAYLOAD:::
param(
    [Alias("h", "?")]
    [switch]$Help,
    [string]$DeviceHost,
    [string]$BrokerHost,
    [int]$BrokerPort = 1883,
    [string]$Topic = "#",
    [string]$Username,
    [string]$Password,
    [switch]$SaveSecrets,
    [string]$OutputRoot = "logs/mqtt-diagnostics",
    [int]$DurationSeconds,
    [string]$MosquittoSubPath,
    [switch]$SkipToolInstall,
    [ValidateRange(2, 60)]
    [int]$TelnetConnectTimeoutSeconds = 6,
    [ValidateRange(250, 60000)]
    [int]$TelnetReconnectDelayMilliseconds = 500,
    [ValidateRange(250, 60000)]
    [int]$TelnetPostDisconnectDelayMilliseconds = 1000,
    [switch]$SkipBrowserCapture,
    [string]$BrowserUrl,
    [int]$BrowserDebugPort = 9222,
    [string]$BrowserPath,
    [switch]$SkipHttpProbes,
    [ValidateRange(1, 60)]
    [int]$HttpProbeTimeoutSeconds = 10
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$script:SummaryPath = $null
$script:SummaryLines = New-Object System.Collections.Generic.List[string]
$script:CaptureStopReason = $null
$telnetPort = 23
$telnetConnectTimeoutSeconds = $TelnetConnectTimeoutSeconds
$telnetPostDisconnectDelayMilliseconds = $TelnetPostDisconnectDelayMilliseconds
$telnetReconnectDelayMilliseconds = $TelnetReconnectDelayMilliseconds

# Telnet debug toggles are parsed dynamically from the banner "Debug toggles"
# block, so the same code works across firmware families. 1.x and 2.0.0/OTGW32
# use different keys (1.x: 4=MQTTGate, 6=NTP; 2.0.0: g=MQTTGate, n=NTP, plus
# 5=SAT, 6=OTDirect, 7=SATBLE). Each toggle is a press-key-to-flip switch, so we
# only send the key when its state is [0]. Simulator toggles inject fake data,
# not log, and are excluded by label.
$script:DebugToggleSimulators = @('SensorSim', 'OTGW-Sim')

function Show-Help {
    Write-Host "OTGW MQTT diagnostic capture"
    Write-Host ""
    Write-Host "Usage:"
    Write-Host "  .\scripts\capture-mqtt-debug.bat [-DeviceHost <host>] [-BrokerHost <host>] [-BrokerPort <port>] [-Topic <topic>] [-Username <user>] [-Password <pass>] [-DurationSeconds <seconds>]"
    Write-Host "  .\scripts\capture-mqtt-debug.bat --help"
    Write-Host ""
    Write-Host "Debug logging:"
    Write-Host "  On connect (and after every reconnect/reboot) the script enables ALL telnet logging"
    Write-Host "  toggles that are off: OTmsg, REST API, MQTT, MQTTGate, Sensors, NTP. Toggles already"
    Write-Host "  on are left as-is. Simulator toggles (SensorSim, OTGW-Sim) are never touched. Toggle keys"
    Write-Host "  are parsed from the banner, so 1.x and 2.0.0/OTGW32 layouts both work."
    Write-Host "  It then sends 'q' (read settings) and 'D' (dump settings/state) so they land in telnet.log."
    Write-Host ""
    Write-Host "Interactive mode:"
    Write-Host "  If DeviceHost or BrokerHost is omitted, the script prompts for the OTGW device host and MQTT broker host."
    Write-Host "  It then prompts for an optional MQTT username. Leave it blank for anonymous brokers."
    Write-Host "  If a username is supplied without -Password, the script prompts securely for the MQTT password."
    Write-Host "  Host/broker/username answers are remembered and pre-filled next run ([default]; Enter accepts)."
    Write-Host "  The MQTT password is never saved."
    Write-Host ""
    Write-Host "Browser devtools capture (console + exceptions + resource 404s + network timings):"
    Write-Host "  Enabled by default. A headless Microsoft Edge (or Chrome) instance loads the OTGW web UI over the"
    Write-Host "  Chrome DevTools Protocol and its console/network output is written to browser.log, merged into transcript.txt."
    Write-Host "  -SkipBrowserCapture            Disable the browser capture entirely (telnet + MQTT only)."
    Write-Host "  -BrowserUrl <url>             Page to load (default http://<DeviceHost>/)."
    Write-Host "  -BrowserDebugPort <port>     CDP remote-debugging port (default 9222; auto-bumped if busy)."
    Write-Host "  -BrowserPath <path>          Explicit msedge.exe/chrome.exe path (default: auto-detect)."
    Write-Host ""
    Write-Host "HTTP probes (curl):"
    Write-Host "  After the live capture stops (connection broken), the script runs a short series of curl.exe"
    Write-Host "  requests against the device (root page + key REST endpoints incl. /api/v2/device/info) and writes"
    Write-Host "  the per-request status/time/size to curl-probes.log, merged into transcript.txt. Each request is"
    Write-Host "  time-bounded so a hanging endpoint is recorded as TIMEOUT instead of stalling the script."
    Write-Host "  -SkipHttpProbes              Disable the post-capture curl probes."
    Write-Host "  -HttpProbeTimeoutSeconds <n> Per-request timeout in seconds (default 10, range 1-60)."
    Write-Host ""
    Write-Host "Stopping capture:"
    Write-Host "  Press Q in the console to stop cleanly. The script closes the logs and leaves transcript.txt."
    Write-Host "  Ctrl+C and Ctrl+Break remain fallback interrupts, but cmd.exe may show its batch-job prompt after Ctrl+C."
    Write-Host "  Or pass -DurationSeconds <seconds> to stop automatically after a fixed interval."
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "  .\scripts\capture-mqtt-debug.bat"
    Write-Host "  .\scripts\capture-mqtt-debug.bat -DeviceHost 192.168.1.50 -BrokerHost 192.168.1.10 -Username mqttuser"
    Write-Host "  .\scripts\capture-mqtt-debug.bat --help"
}

if ($Help) {
    Show-Help
    exit 0
}

function Add-SummaryLine {
    param([string]$Line)

    $script:SummaryLines.Add($Line) | Out-Null
    if ($script:SummaryPath) {
        Set-Content -Path $script:SummaryPath -Value $script:SummaryLines -Encoding UTF8
    }
}

function Write-CaptureStatus {
    param([string]$Line)

    Add-SummaryLine -Line $Line
    Write-Host $Line
}

function Initialize-CancelFlag {
    if (-not ("OtgMqttCapture.CancelFlag" -as [type])) {
        Add-Type -TypeDefinition @"
using System;
using System.Threading;

namespace OtgMqttCapture
{
    public static class CancelFlag
    {
        private static int stopRequested;
        private static readonly ConsoleCancelEventHandler cancelHandler = OnCancelKeyPress;

        public static ConsoleCancelEventHandler Handler
        {
            get { return cancelHandler; }
        }

        public static bool StopRequested
        {
            get { return Interlocked.CompareExchange(ref stopRequested, 0, 0) != 0; }
        }

        public static void Reset()
        {
            Interlocked.Exchange(ref stopRequested, 0);
        }

        public static void RequestStop()
        {
            Interlocked.Exchange(ref stopRequested, 1);
        }

        private static void OnCancelKeyPress(object sender, ConsoleCancelEventArgs eventArgs)
        {
            eventArgs.Cancel = true;
            RequestStop();
        }
    }
}
"@
    }

    [OtgMqttCapture.CancelFlag]::Reset()
}

function Request-CaptureStop {
    param([Parameter(Mandatory = $true)][string]$Reason)

    if ([string]::IsNullOrWhiteSpace($script:CaptureStopReason)) {
        $script:CaptureStopReason = $Reason
    }

    [OtgMqttCapture.CancelFlag]::RequestStop()
}

function Test-ConsoleStopKey {
    try {
        if ([Console]::IsInputRedirected) {
            return $false
        }

        while ([Console]::KeyAvailable) {
            $keyInfo = [Console]::ReadKey($true)
            if ($keyInfo.Key -eq [ConsoleKey]::Q) {
                Request-CaptureStop -Reason "Q key"
                return $true
            }
        }
    }
    catch {
        return $false
    }

    return $false
}

function Test-CaptureStopRequested {
    if ([OtgMqttCapture.CancelFlag]::StopRequested) {
        return $true
    }

    [void](Test-ConsoleStopKey)
    return [OtgMqttCapture.CancelFlag]::StopRequested
}

function Wait-CaptureDelay {
    param([Parameter(Mandatory = $true)][int]$Milliseconds)

    $deadline = [DateTime]::UtcNow.AddMilliseconds($Milliseconds)
    while (-not (Test-CaptureStopRequested) -and [DateTime]::UtcNow -lt $deadline) {
        $remaining = [int][Math]::Ceiling(($deadline - [DateTime]::UtcNow).TotalMilliseconds)
        if ($remaining -le 0) {
            break
        }

        Start-Sleep -Milliseconds ([Math]::Min(100, $remaining))
    }
}

function ConvertFrom-SecureStringToPlainText {
    param([Parameter(Mandatory = $true)][System.Security.SecureString]$SecureString)

    $ptr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($SecureString)
    try {
        return [Runtime.InteropServices.Marshal]::PtrToStringBSTR($ptr)
    }
    finally {
        [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($ptr)
    }
}

function Get-FullPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path -Path (Get-Location) -ChildPath $Path))
}

function Test-PathEntryContains {
    param(
        [string]$PathValue,
        [Parameter(Mandatory = $true)][string]$Directory
    )

    if ([string]::IsNullOrWhiteSpace($PathValue)) {
        return $false
    }

    $target = [System.IO.Path]::GetFullPath($Directory).TrimEnd('\')
    foreach ($entry in ($PathValue -split ';')) {
        if ([string]::IsNullOrWhiteSpace($entry)) {
            continue
        }

        try {
            $normalized = [System.IO.Path]::GetFullPath($entry.Trim()).TrimEnd('\')
            if ([string]::Equals($normalized, $target, [System.StringComparison]::OrdinalIgnoreCase)) {
                return $true
            }
        }
        catch {
            continue
        }
    }

    return $false
}

function Add-DirectoryToCurrentPath {
    param([Parameter(Mandatory = $true)][string]$Directory)

    if (-not (Test-PathEntryContains -PathValue $env:Path -Directory $Directory)) {
        $env:Path = "$Directory;$env:Path"
        return $true
    }

    return $false
}

function Add-DirectoryToUserPath {
    param([Parameter(Mandatory = $true)][string]$Directory)

    $machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")

    if ((Test-PathEntryContains -PathValue $machinePath -Directory $Directory) -or
        (Test-PathEntryContains -PathValue $userPath -Directory $Directory)) {
        return $false
    }

    if ([string]::IsNullOrWhiteSpace($userPath)) {
        $newUserPath = $Directory
    }
    else {
        $newUserPath = "$userPath;$Directory"
    }

    [Environment]::SetEnvironmentVariable("Path", $newUserPath, "User")
    return $true
}

function Update-ProcessPathFromEnvironment {
    $machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $parts = @()

    if (-not [string]::IsNullOrWhiteSpace($machinePath)) {
        $parts += $machinePath
    }

    if (-not [string]::IsNullOrWhiteSpace($userPath)) {
        $parts += $userPath
    }

    if ($parts.Count -gt 0) {
        $env:Path = ($parts -join ';')
    }
}

function Get-CommonMosquittoPaths {
    $paths = New-Object System.Collections.Generic.List[string]
    $roots = @(
        $env:ProgramFiles,
        ${env:ProgramFiles(x86)}
    )

    foreach ($root in $roots) {
        if ([string]::IsNullOrWhiteSpace($root)) {
            continue
        }

        $paths.Add((Join-Path -Path $root -ChildPath "mosquitto\mosquitto_sub.exe")) | Out-Null
    }

    $paths.Add("C:\Program Files\mosquitto\mosquitto_sub.exe") | Out-Null
    $paths.Add("C:\Program Files (x86)\mosquitto\mosquitto_sub.exe") | Out-Null
    return $paths | Select-Object -Unique
}

function Find-MosquittoSub {
    param([string]$ExplicitPath)

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        $fullPath = Get-FullPath -Path $ExplicitPath
        if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
            throw "Explicit -MosquittoSubPath was not found: $fullPath"
        }

        return [PSCustomObject]@{
            Path   = $fullPath
            Source = "explicit"
        }
    }

    $command = Get-Command -Name "mosquitto_sub" -ErrorAction SilentlyContinue
    if ($command -and $command.Source) {
        return [PSCustomObject]@{
            Path   = $command.Source
            Source = "PATH"
        }
    }

    foreach ($path in Get-CommonMosquittoPaths) {
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            return [PSCustomObject]@{
                Path   = $path
                Source = "common install path"
            }
        }
    }

    return $null
}

function Install-Mosquitto {
    $winget = Get-Command -Name "winget" -ErrorAction SilentlyContinue
    if (-not $winget -or -not $winget.Source) {
        throw "mosquitto_sub was not found and winget is not available. Install Mosquitto manually or pass -MosquittoSubPath."
    }

    Add-SummaryLine "Mosquitto install: winget install started"
    & $winget.Source install --id EclipseMosquitto.Mosquitto -e --accept-package-agreements --accept-source-agreements
    if ($LASTEXITCODE -ne 0) {
        throw "winget failed to install EclipseMosquitto.Mosquitto (exit code $LASTEXITCODE)."
    }

    Add-SummaryLine "Mosquitto install: winget install completed"
}

function Resolve-MosquittoSub {
    param(
        [string]$ExplicitPath,
        [switch]$SkipInstall
    )

    $candidate = Find-MosquittoSub -ExplicitPath $ExplicitPath
    if ($candidate) {
        $currentPathAdded = $false
        $userPathAdded = $false

        if ($candidate.Source -eq "common install path") {
            $toolDirectory = Split-Path -Path $candidate.Path -Parent
            $currentPathAdded = Add-DirectoryToCurrentPath -Directory $toolDirectory
            $userPathAdded = Add-DirectoryToUserPath -Directory $toolDirectory
        }

        return [PSCustomObject]@{
            Path             = $candidate.Path
            Source           = $candidate.Source
            Installed        = $false
            CurrentPathAdded = $currentPathAdded
            UserPathAdded    = $userPathAdded
        }
    }

    if ($SkipInstall) {
        throw "mosquitto_sub was not found. Remove -SkipToolInstall, install Mosquitto, or pass -MosquittoSubPath."
    }

    Install-Mosquitto
    Update-ProcessPathFromEnvironment
    $candidate = Find-MosquittoSub
    if (-not $candidate) {
        throw "Mosquitto installation completed, but mosquitto_sub.exe still could not be found."
    }

    $toolDirectory = Split-Path -Path $candidate.Path -Parent
    $currentPathAdded = Add-DirectoryToCurrentPath -Directory $toolDirectory
    $userPathAdded = Add-DirectoryToUserPath -Directory $toolDirectory

    return [PSCustomObject]@{
        Path             = $candidate.Path
        Source           = $candidate.Source
        Installed        = $true
        CurrentPathAdded = $currentPathAdded
        UserPathAdded    = $userPathAdded
    }
}

function ConvertTo-CommandLineArgument {
    param([Parameter(Mandatory = $true)][string]$Value)

    if ($Value -notmatch '[\s"]') {
        return $Value
    }

    return '"' + ($Value -replace '(\\*)"', '$1$1\"' -replace '(\\+)$', '$1$1') + '"'
}

function Start-MosquittoSub {
    param(
        [Parameter(Mandatory = $true)][string]$ExecutablePath,
        [Parameter(Mandatory = $true)][string]$MqttLog,
        [Parameter(Mandatory = $true)][string]$MqttErrorLog,
        [Parameter(Mandatory = $true)][string]$HostName,
        [Parameter(Mandatory = $true)][int]$Port,
        [Parameter(Mandatory = $true)][string]$SubscriptionTopic,
        [string]$UserName,
        [string]$PlainPassword
    )

    $arguments = New-Object System.Collections.Generic.List[string]
    $arguments.Add("-h") | Out-Null
    $arguments.Add($HostName) | Out-Null
    $arguments.Add("-p") | Out-Null
    $arguments.Add([string]$Port) | Out-Null
    $arguments.Add("-t") | Out-Null
    $arguments.Add($SubscriptionTopic) | Out-Null
    $arguments.Add("-v") | Out-Null

    if (-not [string]::IsNullOrWhiteSpace($UserName)) {
        $arguments.Add("-u") | Out-Null
        $arguments.Add($UserName) | Out-Null

        if ($null -ne $PlainPassword) {
            $arguments.Add("-P") | Out-Null
            $arguments.Add($PlainPassword) | Out-Null
        }
    }

    $quotedArguments = ($arguments | ForEach-Object { ConvertTo-CommandLineArgument -Value $_ }) -join " "
    return Start-Process -FilePath $ExecutablePath -ArgumentList $quotedArguments -NoNewWindow -PassThru -RedirectStandardOutput $MqttLog -RedirectStandardError $MqttErrorLog
}

function New-Utf8Writer {
    param([Parameter(Mandatory = $true)][string]$Path)

    $utf8NoBom = New-Object System.Text.UTF8Encoding -ArgumentList $false
    return New-Object System.IO.StreamWriter -ArgumentList $Path, $true, $utf8NoBom
}

function Invoke-HttpProbes {
    # Post-capture sanity sweep: hit the root page and a handful of REST endpoints
    # with real curl.exe (NOT PowerShell's Invoke-WebRequest alias, which throws on
    # 4xx and confuses status reporting). Each request is time-bounded with --max-time
    # so a hanging endpoint (e.g. a stalled chunked "/" response) is recorded as a
    # TIMEOUT line instead of blocking the script. Runs after the live capture stops.
    param(
        [Parameter(Mandatory = $true)][string]$DeviceHost,
        [Parameter(Mandatory = $true)][string]$ProbeLog,
        [int]$TimeoutSeconds = 10,
        [string[]]$Paths
    )

    $writer = New-Utf8Writer -Path $ProbeLog
    try {
        $curl = (Get-Command curl.exe -ErrorAction SilentlyContinue).Source
        if (-not $curl) {
            $writer.WriteLine("curl.exe not found on PATH; HTTP probes skipped.")
            Add-SummaryLine "HTTP probes: skipped (curl.exe not found)."
            return
        }

        $writer.WriteLine("HTTP probes via $curl")
        $writer.WriteLine("Target host : $DeviceHost")
        $writer.WriteLine("Per-request timeout: ${TimeoutSeconds}s (--max-time)")
        $writer.WriteLine("Started: $((Get-Date).ToString('o'))")
        $writer.WriteLine("Columns: <time>  <result>  <http_code> <time_total> <size> <content_type>  <url>")
        $writer.WriteLine("")

        foreach ($path in $Paths) {
            if ([string]::IsNullOrWhiteSpace($path)) { continue }
            $url = "http://$DeviceHost$path"
            $bodyTmp = [System.IO.Path]::GetTempFileName()
            $stamp = (Get-Date).ToString('HH:mm:ss.fff')
            try {
                $curlArgs = @(
                    '--silent', '--show-error',
                    '--max-time', "$TimeoutSeconds",
                    '--output', $bodyTmp,
                    '--write-out', '%{http_code} %{time_total}s %{size_download}B %{content_type}',
                    $url
                )
                $out = & $curl @curlArgs 2>&1
                $exit = $LASTEXITCODE
                $metrics = ($out | Out-String).Trim()

                $snippet = ""
                if (Test-Path -LiteralPath $bodyTmp -PathType Leaf) {
                    $raw = Get-Content -LiteralPath $bodyTmp -Raw -ErrorAction SilentlyContinue
                    if ($raw) {
                        $snippet = ($raw -replace '\s+', ' ').Trim()
                        if ($snippet.Length -gt 200) { $snippet = $snippet.Substring(0, 200) + '...' }
                    }
                }

                if ($exit -eq 0) {
                    $writer.WriteLine("$stamp  OK       $metrics  $url")
                    if ($snippet) { $writer.WriteLine("              body: $snippet") }
                }
                elseif ($exit -eq 28) {
                    $writer.WriteLine("$stamp  TIMEOUT  no response within ${TimeoutSeconds}s [curl exit 28]  $url")
                }
                else {
                    $writer.WriteLine("$stamp  ERROR    curl exit $exit  $metrics  $url")
                }
            }
            catch {
                $writer.WriteLine("$stamp  EXCEPTION  $($_.Exception.Message)  $url")
            }
            finally {
                Remove-Item -LiteralPath $bodyTmp -Force -ErrorAction SilentlyContinue
            }
        }

        $writer.WriteLine("")
        $writer.WriteLine("Finished: $((Get-Date).ToString('o'))")
        Add-SummaryLine "HTTP probes: ran $(@($Paths).Count) request(s) -> curl-probes.log"
    }
    finally {
        $writer.Flush()
        $writer.Dispose()
        # A probe TIMEOUT/ERROR is captured data, not a tool failure. Reset the native
        # exit code so a non-zero curl exit (e.g. 28) does not leak into the script's
        # overall exit code via pwsh -File. Per-probe outcomes are logged above.
        $global:LASTEXITCODE = 0
    }
}

function New-MergedTranscript {
    param([Parameter(Mandatory = $true)][string]$RunPath)

    # Combine every capture file into ONE transcript so the tester uploads a
    # single file. Order: summary first (metadata + event timeline), then the
    # raw streams. Each file is read defensively so a missing/locked file cannot
    # abort the whole merge.
    #
    # Naming convention (shared with capture-serial.py):
    #   transcript-<hostname>-<uniqueid>-<hardware>-<datetime>.txt
    # Identity is parsed from the captured telnet banner / 'D' state dump; each
    # field falls back to a safe default when the device did not report it.
    $idHost = "OTGW"; $idUid = "unknown"; $idHw = "esp32"
    $telnetPath = Join-Path -Path $RunPath -ChildPath "telnet.log"
    if (Test-Path -LiteralPath $telnetPath -PathType Leaf) {
        $bannerLines = Get-Content -LiteralPath $telnetPath -TotalCount 600 -ErrorAction SilentlyContinue
        foreach ($line in $bannerLines) {
            if     ($line -match 'Host\s*:\s*(\S+)')          { $idHost = $Matches[1] }
            elseif ($line -match 'unique_id:\s*(\S+)')        { $idUid  = $Matches[1] }
            elseif ($line -match 'MQTT uniqueid\s*:\s*(\S+)') { $idUid  = $Matches[1] }
            elseif ($line -match 'hardware\.mode:\s*(\S+)')   { $idHw   = $Matches[1] }
        }
    }
    $sanitize = { param($s) (($s -replace '[^A-Za-z0-9._-]', '_')) }
    $idHost = & $sanitize $idHost; $idUid = & $sanitize $idUid; $idHw = & $sanitize $idHw
    $stamp = (Get-Date).ToString('yyyyMMdd-HHmmss')
    $mergedName = "transcript-$idHost-$idUid-$idHw-$stamp.txt"
    $mergedPath = Join-Path -Path $RunPath -ChildPath $mergedName
    $parts = @(
        @{ Title = "SUMMARY (summary.txt)";          File = "summary.txt" },
        @{ Title = "OTGW TELNET DEBUG (telnet.log)";  File = "telnet.log" },
        @{ Title = "MQTT BROKER STREAM (mqtt.log)";   File = "mqtt.log" },
        @{ Title = "MQTT STDERR (mqtt.stderr.log)";   File = "mqtt.stderr.log" },
        @{ Title = "BROWSER DEVTOOLS (browser.log)";  File = "browser.log" },
        @{ Title = "HTTP PROBES (curl-probes.log)";   File = "curl-probes.log" }
    )

    $utf8NoBom = New-Object System.Text.UTF8Encoding -ArgumentList $false
    $writer = New-Object System.IO.StreamWriter -ArgumentList $mergedPath, $false, $utf8NoBom
    try {
        $writer.WriteLine("OTGW capture - merged transcript")
        $writer.WriteLine("Generated: $((Get-Date).ToString('o'))")
        $writer.WriteLine("Run folder: $RunPath")
        $writer.WriteLine("Upload THIS single file; it contains every capture log below.")
        $writer.WriteLine("")

        foreach ($part in $parts) {
            $path = Join-Path -Path $RunPath -ChildPath $part.File
            $writer.WriteLine("============================================================")
            $writer.WriteLine("=== $($part.Title)")
            $writer.WriteLine("============================================================")
            if (Test-Path -LiteralPath $path -PathType Leaf) {
                try {
                    $content = [System.IO.File]::ReadAllText($path)
                    $writer.Write($content)
                    if (-not $content.EndsWith("`n")) {
                        $writer.WriteLine("")
                    }
                }
                catch {
                    $writer.WriteLine("(could not read: $($_.Exception.Message))")
                }
            }
            else {
                $writer.WriteLine("(not present)")
            }
            $writer.WriteLine("")
        }
    }
    finally {
        $writer.Flush()
        $writer.Dispose()
    }

    return $mergedPath
}

function Remove-IntermediateCaptureFiles {
    param(
        [Parameter(Mandatory = $true)][string]$RunPath,
        [Parameter(Mandatory = $true)][string]$TranscriptPath
    )

    $transcriptFullPath = [System.IO.Path]::GetFullPath($TranscriptPath)
    $removed = New-Object System.Collections.Generic.List[string]

    foreach ($file in @("summary.txt", "telnet.log", "mqtt.log", "mqtt.stderr.log", "browser.log", "curl-probes.log")) {
        $path = Join-Path -Path $RunPath -ChildPath $file
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            continue
        }

        $fullPath = [System.IO.Path]::GetFullPath($path)
        if ([string]::Equals($fullPath, $transcriptFullPath, [System.StringComparison]::OrdinalIgnoreCase)) {
            continue
        }

        Remove-Item -LiteralPath $path -Force
        $removed.Add($file) | Out-Null
    }

    return $removed
}

function New-TelnetClient {
    param(
        [Parameter(Mandatory = $true)][string]$HostName,
        [Parameter(Mandatory = $true)][int]$Port,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds
    )

    $client = New-Object System.Net.Sockets.TcpClient
    $asyncResult = $null

    try {
        $asyncResult = $client.BeginConnect($HostName, $Port, $null, $null)
        if (-not $asyncResult.AsyncWaitHandle.WaitOne([TimeSpan]::FromSeconds($TimeoutSeconds))) {
            throw "Timed out connecting to $HostName`:$Port after $TimeoutSeconds seconds."
        }

        $client.EndConnect($asyncResult)
        return $client
    }
    catch {
        $client.Close()
        throw
    }
    finally {
        if ($asyncResult -and $asyncResult.AsyncWaitHandle) {
            $asyncResult.AsyncWaitHandle.Close()
        }
    }
}

function Close-TelnetClient {
    param([AllowNull()][System.Net.Sockets.TcpClient]$Client)

    if ($Client) {
        try {
            $Client.Close()
        }
        catch {
        }
    }
}

function Test-TelnetDisconnected {
    param([AllowNull()][System.Net.Sockets.TcpClient]$Client)

    if (-not $Client) {
        return $true
    }

    try {
        if (-not $Client.Connected) {
            return $true
        }

        $socket = $Client.Client
        if (-not $socket) {
            return $true
        }

        return ($socket.Poll(0, [System.Net.Sockets.SelectMode]::SelectRead) -and $socket.Available -eq 0)
    }
    catch {
        return $true
    }
}

function Test-TelnetRebootMarker {
    param([AllowNull()][string]$Text)

    if ([string]::IsNullOrEmpty($Text)) {
        return $false
    }

    return $Text -match 'ESP\.restart\(\)'
}

function Read-TelnetAvailable {
    param(
        [Parameter(Mandatory = $true)][System.Net.Sockets.NetworkStream]$Stream,
        [Parameter(Mandatory = $true)][System.IO.StreamWriter]$Writer
    )

    $buffer = New-Object byte[] 4096
    $text = ""

    while ($Stream.DataAvailable) {
        $read = $Stream.Read($buffer, 0, $buffer.Length)
        if ($read -le 0) {
            break
        }

        $chunk = [System.Text.Encoding]::ASCII.GetString($buffer, 0, $read)
        $Writer.Write($chunk)
        $Writer.Flush()
        $text += $chunk
    }

    return $text
}

function Read-InitialTelnetBanner {
    param(
        [Parameter(Mandatory = $true)][System.Net.Sockets.NetworkStream]$Stream,
        [Parameter(Mandatory = $true)][System.IO.StreamWriter]$Writer,
        [int]$TimeoutSeconds = 6
    )

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $banner = ""

    # Break on the end-of-banner footer (after the full "Debug toggles" block) so
    # every logging toggle 1-6 is present before we parse states. Breaking on the
    # MQTT line (toggle 3) would miss toggles 4-6 printed on the following lines.
    while ((Get-Date) -lt $deadline) {
        if ($Stream.DataAvailable) {
            $banner += Read-TelnetAvailable -Stream $Stream -Writer $Writer
            if ($banner -match "Press 'h' for command menu" -or $banner -match '(?m)\bOTGW-Sim\b') {
                break
            }
        }
        else {
            Start-Sleep -Milliseconds 100
        }
    }

    return $banner
}

function Enable-AllTelnetDebugIfNeeded {
    param(
        [Parameter(Mandatory = $true)][System.Net.Sockets.NetworkStream]$Stream,
        [Parameter(Mandatory = $true)][System.IO.StreamWriter]$Writer,
        [Parameter(Mandatory = $true)][AllowEmptyString()][AllowNull()][string]$Banner
    )

    # Parse the "Debug toggles" block from the connect-time banner dynamically and
    # flip every toggle showing [0] by sending its key. Matching "<key> <Label>
    # [<0|1>]" triplets makes this firmware-agnostic. Status flags such as
    # "[-D---W--]" never match because the bracket must hold a single 0 or 1.
    $results = New-Object System.Collections.Generic.List[string]
    $pattern = '(?:^|\s)(?<key>\S)\s+(?<label>[A-Za-z][A-Za-z0-9 /]*?)\s*\[(?<state>[01])\]'
    $seen = New-Object System.Collections.Generic.HashSet[string]

    foreach ($m in [regex]::Matches($Banner, $pattern, [System.Text.RegularExpressions.RegexOptions]::Multiline)) {
        $key = $m.Groups['key'].Value
        $label = $m.Groups['label'].Value.Trim()
        $state = $m.Groups['state'].Value
        if ([string]::IsNullOrWhiteSpace($label)) { continue }
        if (-not $seen.Add($label)) { continue }

        if ($script:DebugToggleSimulators -contains $label) {
            $results.Add("$label=skipped (simulator)") | Out-Null
            continue
        }

        if ($state -eq "0") {
            $bytes = [System.Text.Encoding]::ASCII.GetBytes($key)
            $Stream.Write($bytes, 0, $bytes.Length)
            $Stream.Flush()
            Start-Sleep -Milliseconds 300
            [void](Read-TelnetAvailable -Stream $Stream -Writer $Writer)
            $results.Add("$label=on (sent '$key')") | Out-Null
        }
        else {
            $results.Add("$label=already-on") | Out-Null
        }
    }

    if ($results.Count -eq 0) {
        return "no debug toggles found in banner"
    }
    return ($results -join "; ")
}

function Send-TelnetKeyAndDrain {
    param(
        [Parameter(Mandatory = $true)][System.Net.Sockets.NetworkStream]$Stream,
        [Parameter(Mandatory = $true)][System.IO.StreamWriter]$Writer,
        [Parameter(Mandatory = $true)][string]$Key,
        [int]$TimeoutSeconds = 5,
        [int]$IdleMilliseconds = 800
    )

    # Send a single command key, then drain the multi-line response into telnet.log
    # until the stream stays quiet for IdleMilliseconds, with a hard timeout cap so
    # a stalled device cannot block capture.
    $bytes = [System.Text.Encoding]::ASCII.GetBytes($Key)
    $Stream.Write($bytes, 0, $bytes.Length)
    $Stream.Flush()

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $idle = 0
    while ((Get-Date) -lt $deadline) {
        if ($Stream.DataAvailable) {
            [void](Read-TelnetAvailable -Stream $Stream -Writer $Writer)
            $idle = 0
        }
        else {
            Start-Sleep -Milliseconds 100
            $idle += 100
            if ($idle -ge $IdleMilliseconds) {
                break
            }
        }
    }
}

function Request-SettingsDump {
    param(
        [Parameter(Mandatory = $true)][System.Net.Sockets.NetworkStream]$Stream,
        [Parameter(Mandatory = $true)][System.IO.StreamWriter]$Writer
    )

    # Capture device settings + state at session start. Firmware command keys:
    #   'q' = force read settings (re-read from filesystem)
    #   'D' = dump full debug info (settings + state) on 2.0.0; full INI dump on 1.x
    # Both stream multi-line output into telnet.log. Unknown keys are ignored by the
    # firmware, so sending both is safe across firmware families.
    Send-TelnetKeyAndDrain -Stream $Stream -Writer $Writer -Key "q"
    Send-TelnetKeyAndDrain -Stream $Stream -Writer $Writer -Key "D"

    return "sent 'q' (read settings) + 'D' (dump settings/state)"
}

function Connect-TelnetCapture {
    param(
        [Parameter(Mandatory = $true)][string]$HostName,
        [Parameter(Mandatory = $true)][int]$Port,
        [Parameter(Mandatory = $true)][System.IO.StreamWriter]$Writer,
        [Parameter(Mandatory = $true)][int]$ConnectTimeoutSeconds
    )

    Add-SummaryLine "Telnet connect started: $HostName`:$Port (timeout ${ConnectTimeoutSeconds}s)"
    $client = New-TelnetClient -HostName $HostName -Port $Port -TimeoutSeconds $ConnectTimeoutSeconds

    try {
        $stream = $client.GetStream()
        Add-SummaryLine "Telnet connected: $((Get-Date).ToString('o'))"

        $banner = Read-InitialTelnetBanner -Stream $stream -Writer $Writer
        $toggleAction = Enable-AllTelnetDebugIfNeeded -Stream $stream -Writer $Writer -Banner $banner
        Add-SummaryLine "Debug toggle actions: $toggleAction"

        $dumpAction = Request-SettingsDump -Stream $stream -Writer $Writer
        Add-SummaryLine "Settings dump: $dumpAction"

        return [PSCustomObject]@{
            Client = $client
            Stream = $stream
        }
    }
    catch {
        Close-TelnetClient -Client $client
        throw
    }
}

function Get-TelnetEffectiveConnectTimeoutSeconds {
    param(
        [Parameter(Mandatory = $true)][int]$BaseTimeoutSeconds,
        [Parameter(Mandatory = $true)][int]$ReconnectAttempts
    )

    # Gradually increase timeout for retries so .local/mDNS and reboot windows can recover.
    $extraSeconds = [Math]::Min([Math]::Max($ReconnectAttempts - 1, 0), 6)
    return [Math]::Min($BaseTimeoutSeconds + $extraSeconds, 20)
}

function Find-Browser {
    param([string]$ExplicitPath)

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        if (Test-Path -LiteralPath $ExplicitPath -PathType Leaf) {
            return $ExplicitPath
        }
        throw "Explicit -BrowserPath was not found: $ExplicitPath"
    }

    foreach ($cmd in @("msedge", "chrome")) {
        $found = Get-Command -Name $cmd -ErrorAction SilentlyContinue
        if ($found -and $found.Source) {
            return $found.Source
        }
    }

    $candidates = @(
        (Join-Path -Path $env:ProgramFiles -ChildPath "Microsoft\Edge\Application\msedge.exe"),
        (Join-Path -Path ${env:ProgramFiles(x86)} -ChildPath "Microsoft\Edge\Application\msedge.exe"),
        (Join-Path -Path $env:ProgramFiles -ChildPath "Google\Chrome\Application\chrome.exe"),
        (Join-Path -Path ${env:ProgramFiles(x86)} -ChildPath "Google\Chrome\Application\chrome.exe")
    )
    foreach ($candidate in $candidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return $candidate
        }
    }

    return $null
}

function Test-TcpPortFree {
    param([int]$Port)

    try {
        $listener = New-Object System.Net.Sockets.TcpListener([System.Net.IPAddress]::Loopback, $Port)
        $listener.Start()
        $listener.Stop()
        return $true
    }
    catch {
        return $false
    }
}

function Select-DebugPort {
    param([int]$Preferred)

    for ($port = $Preferred; $port -lt ($Preferred + 12); $port++) {
        if (Test-TcpPortFree -Port $port) {
            return $port
        }
    }

    return $Preferred
}

# The browser worker runs in its own runspace (separate thread, same process) so it
# can pump the CDP websocket concurrently with the telnet/MQTT capture loop. It shares
# the in-process [OtgMqttCapture.CancelFlag] static for the stop signal, writes its own
# browser.log (no contention with the main thread), and returns a one-line status string
# that the main thread folds into summary.txt.
$script:BrowserWorkerScript = {
    param(
        [string]$BrowserPath,
        [string]$DeviceUrl,
        [int]$DebugPort,
        [string]$TempProfile,
        [string]$BrowserLog
    )

    $utf8NoBom = New-Object System.Text.UTF8Encoding -ArgumentList $false
    $log = New-Object System.IO.StreamWriter -ArgumentList $BrowserLog, $false, $utf8NoBom
    $log.AutoFlush = $true

    function Write-BrowserLine {
        param([string]$Line)
        $log.WriteLine("$((Get-Date).ToString('HH:mm:ss.fff'))  $Line")
    }

    $edge = $null
    $cws = $null
    $script:cdpId = 0
    $summary = "Browser capture: completed normally."

    function Send-Cdp {
        param([string]$Method, $Params)
        $script:cdpId++
        $payload = @{ id = $script:cdpId; method = $Method }
        if ($Params) { $payload.params = $Params }
        $json = $payload | ConvertTo-Json -Compress -Depth 12
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($json)
        $segment = New-Object System.ArraySegment[byte] -ArgumentList (, $bytes)
        [void]$cws.SendAsync($segment, [System.Net.WebSockets.WebSocketMessageType]::Text, $true, [System.Threading.CancellationToken]::None).GetAwaiter().GetResult()
    }

    try {
        Write-BrowserLine "browser executable: $BrowserPath"
        Write-BrowserLine "device url: $DeviceUrl   cdp port: $DebugPort"

        $browserArgs = @(
            "--headless=new",
            "--disable-gpu",
            "--no-first-run",
            "--no-default-browser-check",
            "--disable-extensions",
            "--disable-background-networking",
            "--mute-audio",
            # Silence Chromium's stderr logging sink (task-manager fallback, USB
            # device_event_log, SmartScreen DNS timeout) so it does not mix with
            # the capture status on the console. CDP browser.log is unaffected.
            "--disable-logging",
            "--log-level=3",
            "--disable-features=msSmartScreenProtection",
            "--remote-allow-origins=*",
            "--remote-debugging-port=$DebugPort",
            "--user-data-dir=$TempProfile",
            "about:blank"
        )
        $edge = Start-Process -FilePath $BrowserPath -ArgumentList $browserArgs -PassThru -WindowStyle Hidden

        # Attach to the about:blank page target FIRST, then navigate, so page-load console
        # logs and resource requests are captured from the very start.
        $wsUrl = $null
        $deadline = (Get-Date).AddSeconds(15)
        while ((Get-Date) -lt $deadline -and -not [OtgMqttCapture.CancelFlag]::StopRequested) {
            try {
                $raw = (New-Object System.Net.WebClient).DownloadString("http://127.0.0.1:$DebugPort/json")
                $targets = $raw | ConvertFrom-Json
                $page = $targets | Where-Object { $_.type -eq "page" -and $_.webSocketDebuggerUrl } | Select-Object -First 1
                if ($page) {
                    $wsUrl = $page.webSocketDebuggerUrl
                    break
                }
            }
            catch {
                Start-Sleep -Milliseconds 300
            }
            Start-Sleep -Milliseconds 200
        }

        if ([string]::IsNullOrWhiteSpace($wsUrl)) {
            throw "CDP page target not found on port $DebugPort within 15s"
        }
        Write-BrowserLine "cdp websocket: $wsUrl"

        $cws = New-Object System.Net.WebSockets.ClientWebSocket
        $cws.ConnectAsync([Uri]$wsUrl, [System.Threading.CancellationToken]::None).GetAwaiter().GetResult()
        Write-BrowserLine "cdp connected"

        Send-Cdp -Method "Network.enable"
        Send-Cdp -Method "Runtime.enable"
        Send-Cdp -Method "Log.enable"
        Send-Cdp -Method "Page.enable"
        Send-Cdp -Method "Page.navigate" -Params @{ url = $DeviceUrl }
        Write-BrowserLine "domains enabled; navigating to $DeviceUrl"

        $requests = @{}
        $buffer = New-Object byte[] 16384
        $segment = New-Object System.ArraySegment[byte] -ArgumentList (, $buffer)

        while (-not [OtgMqttCapture.CancelFlag]::StopRequested) {
            $builder = New-Object System.Text.StringBuilder
            $complete = $false
            $closed = $false

            while (-not $complete) {
                $receiveCts = New-Object System.Threading.CancellationTokenSource
                $receiveCts.CancelAfter(750)
                try {
                    $result = $cws.ReceiveAsync($segment, $receiveCts.Token).GetAwaiter().GetResult()
                }
                catch {
                    # Timeout (idle between messages) or stop: re-check the flag. The websocket
                    # stream position is preserved, so a mid-message timeout simply retries.
                    if ([OtgMqttCapture.CancelFlag]::StopRequested) { $closed = $true; break }
                    continue
                }
                finally {
                    $receiveCts.Dispose()
                }

                if ($result.MessageType -eq [System.Net.WebSockets.WebSocketMessageType]::Close) {
                    $closed = $true
                    break
                }

                [void]$builder.Append([System.Text.Encoding]::UTF8.GetString($buffer, 0, $result.Count))
                if ($result.EndOfMessage) { $complete = $true }
            }

            if ($closed) { break }
            if ($builder.Length -eq 0) { continue }

            try {
                $evt = $builder.ToString() | ConvertFrom-Json
            }
            catch {
                continue
            }
            if (-not $evt.method) { continue }

            switch ($evt.method) {
                "Runtime.consoleAPICalled" {
                    $parts = @()
                    foreach ($arg in $evt.params.args) {
                        if ($null -ne $arg.value) { $parts += [string]$arg.value }
                        elseif ($arg.description) { $parts += [string]$arg.description }
                        elseif ($arg.unserializableValue) { $parts += [string]$arg.unserializableValue }
                        else { $parts += "[$($arg.type)]" }
                    }
                    Write-BrowserLine ("[console.$($evt.params.type)] " + ($parts -join " "))
                }
                "Runtime.exceptionThrown" {
                    $details = $evt.params.exceptionDetails
                    $text = $details.exception.description
                    if (-not $text) { $text = $details.text }
                    Write-BrowserLine "[exception] $text"
                }
                "Log.entryAdded" {
                    $entry = $evt.params.entry
                    $where = if ($entry.url) { " ($($entry.url))" } else { "" }
                    Write-BrowserLine "[log.$($entry.level)] $($entry.text)$where"
                }
                "Network.requestWillBeSent" {
                    $requests[$evt.params.requestId] = @{
                        url    = $evt.params.request.url
                        start  = [double]$evt.params.timestamp
                        status = ""
                    }
                }
                "Network.responseReceived" {
                    if ($requests.ContainsKey($evt.params.requestId)) {
                        $requests[$evt.params.requestId].status = [string]$evt.params.response.status
                    }
                }
                "Network.loadingFinished" {
                    $id = $evt.params.requestId
                    if ($requests.ContainsKey($id)) {
                        $record = $requests[$id]
                        $ms = [int]((([double]$evt.params.timestamp) - $record.start) * 1000)
                        Write-BrowserLine ("[net] {0,6} ms  {1,3}  {2}" -f $ms, $record.status, $record.url)
                        $requests.Remove($id)
                    }
                }
                "Network.loadingFailed" {
                    $id = $evt.params.requestId
                    if ($requests.ContainsKey($id)) {
                        $record = $requests[$id]
                        Write-BrowserLine ("[net] FAILED      {0}  {1}" -f $evt.params.errorText, $record.url)
                        $requests.Remove($id)
                    }
                }
            }
        }

        # Flush requests that started but never finished. On the OTGW these are the
        # smoking gun for the single-threaded webserver stalling under load (the XHR
        # latency ramp), so they are far more diagnostic than a silent omission.
        foreach ($pending in $requests.GetEnumerator()) {
            $statusText = if ($pending.Value.status) { $pending.Value.status } else { "no-response" }
            Write-BrowserLine ("[net] PENDING     {0,3}  {1}  (started, never finished)" -f $statusText, $pending.Value.url)
        }

        Write-BrowserLine "stop requested; closing capture"
    }
    catch {
        $summary = "Browser capture: error - $($_.Exception.Message)"
        try { Write-BrowserLine "[worker-error] $($_.Exception.Message)" } catch { }
    }
    finally {
        if ($cws) {
            try { $cws.Abort() } catch { }
            try { $cws.Dispose() } catch { }
        }
        if ($edge -and -not $edge.HasExited) {
            try { Stop-Process -Id $edge.Id -Force -ErrorAction Stop } catch { }
        }
        try { $log.Flush(); $log.Dispose() } catch { }
        if (-not [string]::IsNullOrWhiteSpace($TempProfile)) {
            try {
                if (Test-Path -LiteralPath $TempProfile) {
                    Remove-Item -LiteralPath $TempProfile -Recurse -Force -ErrorAction SilentlyContinue
                }
            }
            catch { }
        }
    }

    return $summary
}

function Start-BrowserCapture {
    param(
        [Parameter(Mandatory = $true)][string]$BrowserPath,
        [Parameter(Mandatory = $true)][string]$DeviceUrl,
        [Parameter(Mandatory = $true)][int]$DebugPort,
        [Parameter(Mandatory = $true)][string]$TempProfile,
        [Parameter(Mandatory = $true)][string]$BrowserLog
    )

    $worker = [powershell]::Create()
    [void]$worker.AddScript($script:BrowserWorkerScript)
    [void]$worker.AddParameter("BrowserPath", $BrowserPath)
    [void]$worker.AddParameter("DeviceUrl", $DeviceUrl)
    [void]$worker.AddParameter("DebugPort", $DebugPort)
    [void]$worker.AddParameter("TempProfile", $TempProfile)
    [void]$worker.AddParameter("BrowserLog", $BrowserLog)
    $async = $worker.BeginInvoke()

    return [PSCustomObject]@{
        Worker = $worker
        Async  = $async
    }
}

function Stop-BrowserCapture {
    param(
        [AllowNull()]$Handle,
        [int]$TimeoutSeconds = 8
    )

    if (-not $Handle) {
        return $null
    }

    $summary = $null
    try {
        if (-not $Handle.Async.AsyncWaitHandle.WaitOne([TimeSpan]::FromSeconds($TimeoutSeconds))) {
            [void]$Handle.Worker.Stop()
        }
        else {
            $output = $Handle.Worker.EndInvoke($Handle.Async)
            if ($output -and $output.Count -gt 0) {
                $summary = [string]$output[$output.Count - 1]
            }
        }
    }
    catch {
        $summary = "Browser capture: stop error - $($_.Exception.Message)"
    }
    finally {
        try { $Handle.Worker.Dispose() } catch { }
    }

    return $summary
}

function Get-CaptureSettingsPath {
    $base = $env:LOCALAPPDATA
    if ([string]::IsNullOrWhiteSpace($base)) { $base = $env:TEMP }
    return (Join-Path -Path $base -ChildPath "OTGW-capture\capture-settings.json")
}

function Import-CaptureSettings {
    # Best-effort: a missing/corrupt file must never abort the capture.
    try {
        $p = Get-CaptureSettingsPath
        if (Test-Path -LiteralPath $p -PathType Leaf) {
            return (Get-Content -LiteralPath $p -Raw | ConvertFrom-Json)
        }
    }
    catch { }
    return $null
}

function Save-CaptureSettings {
    param(
        [string]$DeviceHost,
        [string]$BrokerHost,
        [int]$BrokerPort,
        [string]$Topic,
        [string]$Username,
        [string]$MqttPassword,
        [switch]$PersistPassword
    )

    # Persist last-used values so the next run can pre-fill the prompts. This file
    # lives in %LOCALAPPDATA%\OTGW-capture\ — OUTSIDE the git repo, so it is never
    # synced to GitHub. The MQTT password is written ONLY when -PersistPassword is
    # set (opt-in, for unattended/autonomous runs); otherwise a previously stored
    # secret is preserved untouched. Best-effort: a write failure never aborts the
    # capture.
    try {
        $p = Get-CaptureSettingsPath
        $dir = Split-Path -Path $p -Parent
        if (-not (Test-Path -LiteralPath $dir)) {
            New-Item -ItemType Directory -Path $dir -Force | Out-Null
        }
        # Carry over any previously stored secret unless we are explicitly writing one.
        $pwToWrite = $null
        if ($PersistPassword) {
            $pwToWrite = $MqttPassword
        }
        elseif (Test-Path -LiteralPath $p) {
            try {
                $prev = Get-Content -LiteralPath $p -Raw | ConvertFrom-Json
                if ($prev.PSObject.Properties.Name -contains 'MqttPassword') {
                    $pwToWrite = [string]$prev.MqttPassword
                }
            }
            catch { }
        }
        $obj = [ordered]@{
            DeviceHost = $DeviceHost
            BrokerHost = $BrokerHost
            BrokerPort = $BrokerPort
            Topic      = $Topic
            Username   = $Username
        }
        if (-not [string]::IsNullOrWhiteSpace($pwToWrite)) {
            $obj['MqttPassword'] = $pwToWrite
        }
        [PSCustomObject]$obj | ConvertTo-Json | Set-Content -LiteralPath $p -Encoding UTF8
        return $p
    }
    catch {
        return $null
    }
}

function Read-HostWithDefault {
    param(
        [Parameter(Mandatory = $true)][string]$Prompt,
        [string]$Default
    )

    if (-not [string]::IsNullOrWhiteSpace($Default)) {
        $answer = Read-Host "$Prompt [$Default]"
        if ([string]::IsNullOrWhiteSpace($answer)) { return $Default }
        return $answer
    }
    return (Read-Host $Prompt)
}

$deviceHostWasBound = $PSBoundParameters.ContainsKey('DeviceHost')
$brokerHostWasBound = $PSBoundParameters.ContainsKey('BrokerHost')
$usernameWasBound = $PSBoundParameters.ContainsKey('Username')
$passwordWasBound = $PSBoundParameters.ContainsKey('Password')

# Pre-fill interactive prompts from the last saved run (password excluded).
$savedSettings = Import-CaptureSettings

if ([string]::IsNullOrWhiteSpace($DeviceHost)) {
    $DeviceHost = Read-HostWithDefault -Prompt "OTGW device host" -Default $(if ($savedSettings) { [string]$savedSettings.DeviceHost } else { "" })
}

if ([string]::IsNullOrWhiteSpace($BrokerHost)) {
    $BrokerHost = Read-HostWithDefault -Prompt "MQTT broker host" -Default $(if ($savedSettings) { [string]$savedSettings.BrokerHost } else { "" })
}

if ([string]::IsNullOrWhiteSpace($DeviceHost)) {
    throw "DeviceHost is required."
}

if ([string]::IsNullOrWhiteSpace($BrokerHost)) {
    throw "BrokerHost is required."
}

if (-not $usernameWasBound -and (-not $deviceHostWasBound -or -not $brokerHostWasBound)) {
    $Username = Read-HostWithDefault -Prompt "MQTT username (blank for anonymous)" -Default $(if ($savedSettings) { [string]$savedSettings.Username } else { "" })
}

if ([string]::IsNullOrWhiteSpace($Username)) {
    if ($passwordWasBound -and -not [string]::IsNullOrWhiteSpace($Password)) {
        throw "Username is required when Password is supplied."
    }
}
elseif (-not $passwordWasBound) {
    # Opt-in autonomous mode: reuse a password previously stored (via -SaveSecrets)
    # in the out-of-repo capture-settings.json, so an unattended run does not block
    # on the prompt. Falls back to a secure prompt when no stored secret exists.
    $storedPw = $null
    if ($savedSettings -and ($savedSettings.PSObject.Properties.Name -contains 'MqttPassword')) {
        $storedPw = [string]$savedSettings.MqttPassword
    }
    if (-not [string]::IsNullOrWhiteSpace($storedPw)) {
        $Password = $storedPw
        Write-Host "MQTT password: loaded from capture-settings.json (out-of-repo secret store)."
    }
    else {
        $securePassword = Read-Host "MQTT password for $Username" -AsSecureString
        $Password = ConvertFrom-SecureStringToPlainText -SecureString $securePassword
    }
}

# Remember the resolved settings for next run's prompt pre-fill. The password is
# persisted ONLY when -SaveSecrets is passed (opt-in), and ONLY to the out-of-repo
# capture-settings.json; a previously stored secret is preserved across normal runs.
$savedSettingsPath = Save-CaptureSettings -DeviceHost $DeviceHost -BrokerHost $BrokerHost -BrokerPort $BrokerPort -Topic $Topic -Username $Username -MqttPassword $Password -PersistPassword:$SaveSecrets

$outputRootPath = Get-FullPath -Path $OutputRoot
$runName = Get-Date -Format "yyyyMMdd-HHmmss"
$runPath = Join-Path -Path $outputRootPath -ChildPath $runName
New-Item -Path $runPath -ItemType Directory -Force | Out-Null

$telnetLog = Join-Path -Path $runPath -ChildPath "telnet.log"
$mqttLog = Join-Path -Path $runPath -ChildPath "mqtt.log"
$mqttErrorLog = Join-Path -Path $runPath -ChildPath "mqtt.stderr.log"
$browserLog = Join-Path -Path $runPath -ChildPath "browser.log"
$httpProbeLog = Join-Path -Path $runPath -ChildPath "curl-probes.log"
$script:SummaryPath = Join-Path -Path $runPath -ChildPath "summary.txt"

Add-SummaryLine "OTGW MQTT diagnostic capture"
Add-SummaryLine "Run folder: $runPath"
Add-SummaryLine "Started: $((Get-Date).ToString('o'))"
Add-SummaryLine "DeviceHost: $DeviceHost"
Add-SummaryLine "TelnetPort: $telnetPort"
Add-SummaryLine "TelnetConnectTimeoutSeconds: $telnetConnectTimeoutSeconds"
Add-SummaryLine "TelnetPostDisconnectDelayMilliseconds: $telnetPostDisconnectDelayMilliseconds"
Add-SummaryLine "TelnetReconnectDelayMilliseconds: $telnetReconnectDelayMilliseconds"
Add-SummaryLine "BrokerHost: $BrokerHost"
Add-SummaryLine "BrokerPort: $BrokerPort"
Add-SummaryLine "Topic: $Topic"
Add-SummaryLine "Username supplied: $([bool](-not [string]::IsNullOrWhiteSpace($Username)))"
Add-SummaryLine "DurationSeconds: $(if ($DurationSeconds -gt 0) { $DurationSeconds } else { 'until manual stop' })"
Add-SummaryLine "SkipToolInstall: $([bool]$SkipToolInstall)"
if ($savedSettingsPath) { Add-SummaryLine "Settings prefill saved: $savedSettingsPath" }
Add-SummaryLine "Telnet timeout strategy: adaptive (base + retry backoff, capped at 20s)"

$telnetClient = $null
$stream = $null
$telnetWriter = $null
$mqttProcess = $null
$cancelHandler = $null
$telnetHasConnected = $false
$telnetReconnectAttempts = 0
$telnetReconnectReason = $null
$nextTelnetConnectUtc = [DateTime]::MinValue
$telnetRebootScanBuffer = ""
$browserHandle = $null

try {
    Initialize-CancelFlag
    $cancelHandler = [OtgMqttCapture.CancelFlag]::Handler
    [System.Console]::add_CancelKeyPress($cancelHandler)

    if ($SkipBrowserCapture) {
        Add-SummaryLine "Browser capture: disabled (-SkipBrowserCapture)."
    }
    else {
        try {
            $resolvedBrowser = Find-Browser -ExplicitPath $BrowserPath
            if ([string]::IsNullOrWhiteSpace($resolvedBrowser)) {
                Add-SummaryLine "Browser capture: skipped - no Edge/Chrome found (pass -BrowserPath or -SkipBrowserCapture)."
            }
            else {
                if ([string]::IsNullOrWhiteSpace($BrowserUrl)) {
                    $BrowserUrl = "http://$DeviceHost/"
                }
                $chosenPort = Select-DebugPort -Preferred $BrowserDebugPort
                $browserProfile = Join-Path -Path ([System.IO.Path]::GetTempPath()) -ChildPath ("otgw-edge-" + [System.IO.Path]::GetRandomFileName())
                New-Item -Path $browserProfile -ItemType Directory -Force | Out-Null

                Add-SummaryLine "Browser capture: $resolvedBrowser"
                Add-SummaryLine "Browser url: $BrowserUrl"
                Add-SummaryLine "Browser CDP port: $chosenPort"

                $browserHandle = Start-BrowserCapture `
                    -BrowserPath $resolvedBrowser `
                    -DeviceUrl $BrowserUrl `
                    -DebugPort $chosenPort `
                    -TempProfile $browserProfile `
                    -BrowserLog $browserLog
                Add-SummaryLine "Browser capture started (headless, writing browser.log)."
            }
        }
        catch {
            Add-SummaryLine "Browser capture: failed to start - $($_.Exception.Message)"
            $browserHandle = $null
        }
    }

    $mosquitto = Resolve-MosquittoSub -ExplicitPath $MosquittoSubPath -SkipInstall:$SkipToolInstall
    Add-SummaryLine "mosquitto_sub: $($mosquitto.Path)"
    Add-SummaryLine "mosquitto_sub source: $($mosquitto.Source)"
    Add-SummaryLine "Mosquitto installed by script: $($mosquitto.Installed)"
    Add-SummaryLine "Mosquitto directory added to current PATH: $($mosquitto.CurrentPathAdded)"
    Add-SummaryLine "Mosquitto directory added to user PATH: $($mosquitto.UserPathAdded)"

    $telnetWriter = New-Utf8Writer -Path $telnetLog
    $mqttProcess = Start-MosquittoSub `
        -ExecutablePath $mosquitto.Path `
        -MqttLog $mqttLog `
        -MqttErrorLog $mqttErrorLog `
        -HostName $BrokerHost `
        -Port $BrokerPort `
        -SubscriptionTopic $Topic `
        -UserName $Username `
        -PlainPassword $Password
    Add-SummaryLine "mosquitto_sub started: pid $($mqttProcess.Id)"

    if ($DurationSeconds -gt 0) {
        $deadline = (Get-Date).AddSeconds($DurationSeconds)
    }
    else {
        $deadline = [DateTime]::MaxValue
    }

    Add-SummaryLine "Capture started: $((Get-Date).ToString('o'))"
    Write-Host "Capturing telnet and MQTT output in $runPath"
    Write-Host "Press Q to stop cleanly and leave transcript.txt. Run with -Help for options."
    Write-Host "Ctrl+C and Ctrl+Break also stop capture; Ctrl+C may still trigger a cmd.exe batch-job prompt."

    while (-not (Test-CaptureStopRequested) -and (Get-Date) -lt $deadline) {
        if ($mqttProcess.HasExited) {
            Add-SummaryLine "mosquitto_sub exited during capture with code $($mqttProcess.ExitCode)."
            break
        }

        if (Test-TelnetDisconnected -Client $telnetClient) {
            if ($telnetClient) {
                $telnetReconnectReason = "connection closed"
                $telnetReconnectAttempts = 0
                $nextTelnetConnectUtc = [DateTime]::UtcNow.AddMilliseconds($telnetPostDisconnectDelayMilliseconds)
                Write-CaptureStatus "Telnet disconnected; waiting $($telnetPostDisconnectDelayMilliseconds / 1000)s before reconnect attempts."
                Close-TelnetClient -Client $telnetClient
                $telnetClient = $null
                $stream = $null
            }

            if ([DateTime]::UtcNow -lt $nextTelnetConnectUtc) {
                $remainingMilliseconds = [int][Math]::Ceiling(($nextTelnetConnectUtc - [DateTime]::UtcNow).TotalMilliseconds)
                Wait-CaptureDelay -Milliseconds ([Math]::Min(250, $remainingMilliseconds))
                continue
            }

            try {
                $telnetReconnectAttempts++
                $effectiveConnectTimeoutSeconds = Get-TelnetEffectiveConnectTimeoutSeconds -BaseTimeoutSeconds $telnetConnectTimeoutSeconds -ReconnectAttempts $telnetReconnectAttempts
                if ($telnetHasConnected) {
                    $reasonSuffix = ""
                    if ($telnetReconnectReason) {
                        $reasonSuffix = " after $telnetReconnectReason"
                    }
                    Add-SummaryLine "Telnet reconnect attempt #$telnetReconnectAttempts started$reasonSuffix (timeout ${effectiveConnectTimeoutSeconds}s)."
                }

                $connection = Connect-TelnetCapture `
                    -HostName $DeviceHost `
                    -Port $telnetPort `
                    -Writer $telnetWriter `
                    -ConnectTimeoutSeconds $effectiveConnectTimeoutSeconds
                $telnetClient = $connection.Client
                $stream = $connection.Stream
                $telnetRebootScanBuffer = ""

                if ($telnetHasConnected) {
                    Write-CaptureStatus "Telnet reconnected after $telnetReconnectAttempts attempt(s): $((Get-Date).ToString('o'))"
                }
                else {
                    Write-CaptureStatus "Telnet connected: $((Get-Date).ToString('o'))"
                }

                $telnetHasConnected = $true
                $telnetReconnectAttempts = 0
                $telnetReconnectReason = $null
                $nextTelnetConnectUtc = [DateTime]::MinValue
            }
            catch {
                $nextTelnetConnectUtc = [DateTime]::UtcNow.AddMilliseconds($telnetReconnectDelayMilliseconds)
                Write-CaptureStatus "Telnet connect attempt #$telnetReconnectAttempts failed (timeout ${effectiveConnectTimeoutSeconds}s): $($_.Exception.Message) Retrying in $($telnetReconnectDelayMilliseconds / 1000)s."
                continue
            }
        }

        try {
            $telnetText = Read-TelnetAvailable -Stream $stream -Writer $telnetWriter
            if (-not [string]::IsNullOrEmpty($telnetText)) {
                $telnetRebootScanBuffer += $telnetText
                if ($telnetRebootScanBuffer.Length -gt 512) {
                    $telnetRebootScanBuffer = $telnetRebootScanBuffer.Substring($telnetRebootScanBuffer.Length - 512)
                }

                if (Test-TelnetRebootMarker -Text $telnetRebootScanBuffer) {
                    $telnetReconnectReason = "ESP.restart() marker captured"
                    $telnetReconnectAttempts = 0
                    $nextTelnetConnectUtc = [DateTime]::UtcNow.AddMilliseconds($telnetPostDisconnectDelayMilliseconds)
                    Write-CaptureStatus "Telnet captured ESP.restart(); waiting $($telnetPostDisconnectDelayMilliseconds / 1000)s before reconnect attempts."
                    Close-TelnetClient -Client $telnetClient
                    $telnetClient = $null
                    $stream = $null
                    $telnetRebootScanBuffer = ""
                    continue
                }
            }
        }
        catch {
            $telnetReconnectReason = "read failed"
            $telnetReconnectAttempts = 0
            $nextTelnetConnectUtc = [DateTime]::UtcNow.AddMilliseconds($telnetPostDisconnectDelayMilliseconds)
            Write-CaptureStatus "Telnet read failed: $($_.Exception.Message); waiting $($telnetPostDisconnectDelayMilliseconds / 1000)s before reconnect attempts."
            Close-TelnetClient -Client $telnetClient
            $telnetClient = $null
            $stream = $null
            continue
        }

        Wait-CaptureDelay -Milliseconds 100
    }

    Add-SummaryLine "Capture stop reason: $(if (Test-CaptureStopRequested) { if ($script:CaptureStopReason) { $script:CaptureStopReason } else { 'Ctrl+C/Ctrl+Break' } } elseif ((Get-Date) -ge $deadline) { 'duration elapsed' } else { 'capture loop ended' })"
}
catch {
    Add-SummaryLine "Error: $($_.Exception.Message)"
    throw
}
finally {
    Add-SummaryLine "Stopping: $((Get-Date).ToString('o'))"

    if ($mqttProcess -and -not $mqttProcess.HasExited) {
        try {
            Stop-Process -Id $mqttProcess.Id -Force -ErrorAction Stop
            Add-SummaryLine "mosquitto_sub stopped by script."
        }
        catch {
            Add-SummaryLine "mosquitto_sub stop error: $($_.Exception.Message)"
        }
    }
    elseif ($mqttProcess) {
        Add-SummaryLine "mosquitto_sub exit code: $($mqttProcess.ExitCode)"
    }

    if ($telnetWriter) {
        $telnetWriter.Flush()
        $telnetWriter.Dispose()
    }

    if ($telnetClient) {
        Close-TelnetClient -Client $telnetClient
    }

    if ($browserHandle) {
        # Ensure the worker sees the stop signal even on an error-driven exit, then drain it.
        Request-CaptureStop -Reason "capture shutdown"
        $browserSummary = Stop-BrowserCapture -Handle $browserHandle -TimeoutSeconds 10
        if ($browserSummary) {
            Add-SummaryLine $browserSummary
        }
    }

    # Post-capture HTTP probes: now that telnet/browser/MQTT are torn down, sweep the
    # device's web endpoints with curl so we can see which respond and which hang.
    if (-not $SkipHttpProbes) {
        try {
            $httpProbePaths = @(
                '/',                         # root page (chunked index emitter - the suspect)
                '/index.html',
                '/index.js',
                '/api/v2/health',
                '/api/v2/device/info',       # the real devinfo endpoint (NOT /api/v2/devinfo)
                '/api/v2/device/time',
                '/api/v2/flash/status',
                '/api/v2/sat/status'
            )
            Write-Host "Running post-capture HTTP probes (curl) -> curl-probes.log"
            Invoke-HttpProbes -DeviceHost $DeviceHost -ProbeLog $httpProbeLog -TimeoutSeconds $HttpProbeTimeoutSeconds -Paths $httpProbePaths
        }
        catch {
            Add-SummaryLine "HTTP probes: failed - $($_.Exception.Message)"
        }
    }
    else {
        Add-SummaryLine "HTTP probes: disabled (-SkipHttpProbes)."
    }

    if ($cancelHandler) {
        [System.Console]::remove_CancelKeyPress($cancelHandler)
    }
    Add-SummaryLine "Finished: $((Get-Date).ToString('o'))"

    try {
        $mergedTranscript = New-MergedTranscript -RunPath $runPath
        $removedFiles = Remove-IntermediateCaptureFiles -RunPath $runPath -TranscriptPath $mergedTranscript
        Write-Host "Merged transcript (single upload): $mergedTranscript"
        if ($removedFiles.Count -gt 0) {
            Write-Host "Removed intermediate capture files: $($removedFiles -join ', ')"
        }
    }
    catch {
        Write-Host "Could not build merged transcript: $($_.Exception.Message)"
    }

    Write-Host "Diagnostic capture folder: $runPath"
}
