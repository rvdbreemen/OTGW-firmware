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
    [string]$OutputRoot = "logs/mqtt-diagnostics",
    [int]$DurationSeconds,
    [string]$MosquittoSubPath,
    [switch]$SkipToolInstall,
    [ValidateRange(2, 60)]
    [int]$TelnetConnectTimeoutSeconds = 6,
    [ValidateRange(250, 60000)]
    [int]$TelnetReconnectDelayMilliseconds = 500,
    [ValidateRange(250, 60000)]
    [int]$TelnetPostDisconnectDelayMilliseconds = 1000
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

function Show-Help {
    Write-Host "OTGW MQTT diagnostic capture"
    Write-Host ""
    Write-Host "Usage:"
    Write-Host "  .\scripts\capture-mqtt-debug.bat [-DeviceHost <host>] [-BrokerHost <host>] [-BrokerPort <port>] [-Topic <topic>] [-Username <user>] [-Password <pass>] [-DurationSeconds <seconds>]"
    Write-Host "  .\scripts\capture-mqtt-debug.bat --help"
    Write-Host ""
    Write-Host "Interactive mode:"
    Write-Host "  If DeviceHost or BrokerHost is omitted, the script prompts for the OTGW device host and MQTT broker host."
    Write-Host "  It then prompts for an optional MQTT username. Leave it blank for anonymous brokers."
    Write-Host "  If a username is supplied without -Password, the script prompts securely for the MQTT password."
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

function New-MergedTranscript {
    param([Parameter(Mandatory = $true)][string]$RunPath)

    # Combine every capture file into ONE transcript so the tester uploads a
    # single file. Order: summary first (metadata + event timeline), then the
    # raw streams. Each file is read defensively so a missing/locked file cannot
    # abort the whole merge.
    $mergedPath = Join-Path -Path $RunPath -ChildPath "transcript.txt"
    $parts = @(
        @{ Title = "SUMMARY (summary.txt)";         File = "summary.txt" },
        @{ Title = "OTGW TELNET DEBUG (telnet.log)"; File = "telnet.log" },
        @{ Title = "MQTT BROKER STREAM (mqtt.log)";  File = "mqtt.log" },
        @{ Title = "MQTT STDERR (mqtt.stderr.log)";  File = "mqtt.stderr.log" }
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

    foreach ($file in @("summary.txt", "telnet.log", "mqtt.log", "mqtt.stderr.log")) {
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

    while ((Get-Date) -lt $deadline) {
        if ($Stream.DataAvailable) {
            $banner += Read-TelnetAvailable -Stream $Stream -Writer $Writer
            if ($banner -match '(?m)\b3\s+MQTT\s+\[(?<state>[01])\]') {
                break
            }
        }
        else {
            Start-Sleep -Milliseconds 100
        }
    }

    return $banner
}

function Enable-MqttTelnetDebugIfNeeded {
    param(
        [Parameter(Mandatory = $true)][System.Net.Sockets.NetworkStream]$Stream,
        [Parameter(Mandatory = $true)][System.IO.StreamWriter]$Writer,
        [Parameter(Mandatory = $true)][AllowEmptyString()][AllowNull()][string]$Banner
    )

    if ($Banner -match '(?m)\b3\s+MQTT\s+\[(?<state>[01])\]') {
        $state = $Matches['state']
        if ($state -eq "0") {
            $bytes = [System.Text.Encoding]::ASCII.GetBytes("3")
            $Stream.Write($bytes, 0, $bytes.Length)
            $Stream.Flush()
            Start-Sleep -Milliseconds 500
            [void](Read-TelnetAvailable -Stream $Stream -Writer $Writer)
            return "sent key 3 because MQTT debug was off"
        }

        return "not sent because MQTT debug was already on"
    }

    return "not sent because MQTT debug state was not found in the telnet banner"
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
        $toggleAction = Enable-MqttTelnetDebugIfNeeded -Stream $stream -Writer $Writer -Banner $banner
        Add-SummaryLine "MQTT debug toggle action: $toggleAction"

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

$deviceHostWasBound = $PSBoundParameters.ContainsKey('DeviceHost')
$brokerHostWasBound = $PSBoundParameters.ContainsKey('BrokerHost')
$usernameWasBound = $PSBoundParameters.ContainsKey('Username')
$passwordWasBound = $PSBoundParameters.ContainsKey('Password')

if ([string]::IsNullOrWhiteSpace($DeviceHost)) {
    $DeviceHost = Read-Host "OTGW device host"
}

if ([string]::IsNullOrWhiteSpace($BrokerHost)) {
    $BrokerHost = Read-Host "MQTT broker host"
}

if ([string]::IsNullOrWhiteSpace($DeviceHost)) {
    throw "DeviceHost is required."
}

if ([string]::IsNullOrWhiteSpace($BrokerHost)) {
    throw "BrokerHost is required."
}

if (-not $usernameWasBound -and (-not $deviceHostWasBound -or -not $brokerHostWasBound)) {
    $Username = Read-Host "MQTT username (blank for anonymous)"
}

if ([string]::IsNullOrWhiteSpace($Username)) {
    if ($passwordWasBound -and -not [string]::IsNullOrWhiteSpace($Password)) {
        throw "Username is required when Password is supplied."
    }
}
elseif (-not $passwordWasBound) {
    $securePassword = Read-Host "MQTT password for $Username" -AsSecureString
    $Password = ConvertFrom-SecureStringToPlainText -SecureString $securePassword
}

$outputRootPath = Get-FullPath -Path $OutputRoot
$runName = Get-Date -Format "yyyyMMdd-HHmmss"
$runPath = Join-Path -Path $outputRootPath -ChildPath $runName
New-Item -Path $runPath -ItemType Directory -Force | Out-Null

$telnetLog = Join-Path -Path $runPath -ChildPath "telnet.log"
$mqttLog = Join-Path -Path $runPath -ChildPath "mqtt.log"
$mqttErrorLog = Join-Path -Path $runPath -ChildPath "mqtt.stderr.log"
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

try {
    Initialize-CancelFlag
    $cancelHandler = [OtgMqttCapture.CancelFlag]::Handler
    [System.Console]::add_CancelKeyPress($cancelHandler)

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
