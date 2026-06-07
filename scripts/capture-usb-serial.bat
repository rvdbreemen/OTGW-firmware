@echo off
rem ============================================================================
rem  capture-usb-serial.bat  -  OTGW ESP8266 USB serial crash-dump capture (Windows)
rem
rem  Captures the ESP8266 UART0 serial stream over USB and logs it, flagging the
rem  SDK panic dump (exception cause + epc1/epc2/epc3/excvaddr + >>>stack>>> trace)
rem  into a separate crash-frames.log. Use this when the network crashlog endpoint
rem  (capture-mqtt-debug.bat / /api/v2/device/crashlog) gives an epc1 that is not
rem  conclusive: the full stack trace only reaches UART0, it is never persisted to
rem  flash, so a live USB capture is the only way to get it.
rem
rem  SINGLE-FILE DELIVERY: this .bat is the only file you need. The PowerShell
rem  worker is embedded at the bottom (after the PSPAYLOAD marker) and extracted to
rem  a temporary .ps1 at run time. Same launcher pattern as capture-mqtt-debug.bat.
rem
rem  Usage:
rem    capture-usb-serial.bat                  (auto-detect COM port, 115200 baud)
rem    capture-usb-serial.bat --help
rem    capture-usb-serial.bat -Port COM5 -Baud 115200
rem    capture-usb-serial.bat -Baud 74880      (boot-ROM reset-cause banner)
rem  All arguments are forwarded verbatim to the embedded PowerShell script.
rem
rem  NOTE: this holds the COM port exclusively. OTmonitor and firmware flashing
rem  cannot use the port while this capture is running. Stop with Q first.
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
set "PS1=%TEMP%\capture-usb-serial-%TEMP_STAMP%-%RANDOM%%RANDOM%%RANDOM%.ps1"
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
    [string]$Port,
    [ValidateRange(300, 921600)]
    [int]$Baud = 115200,
    [string]$OutputRoot = "logs/usb-serial",
    [int]$DurationSeconds
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Show-Help {
    Write-Host "OTGW ESP8266 USB serial crash-dump capture"
    Write-Host ""
    Write-Host "Usage:"
    Write-Host "  .\scripts\capture-usb-serial.bat [-Port COMn] [-Baud <rate>] [-DurationSeconds <seconds>]"
    Write-Host "  .\scripts\capture-usb-serial.bat --help"
    Write-Host ""
    Write-Host "What it does:"
    Write-Host "  Opens the ESP8266 USB serial port and logs everything to usb-serial.log."
    Write-Host "  The ESP panic handler prints the crash dump (Fatal exception N, epc1/epc2/epc3,"
    Write-Host "  excvaddr, and the >>>stack>>> ... <<<stack<<< trace) to UART0 at the moment of the"
    Write-Host "  crash. Those frames are ALSO mirrored into crash-frames.log so they are easy to find"
    Write-Host "  in a long capture. Decode them with the matching firmware .elf:"
    Write-Host "    xtensa-lx106-elf-addr2line -e firmware.elf 0x<epc1>"
    Write-Host ""
    Write-Host "Port selection:"
    Write-Host "  If -Port is omitted the script auto-detects a USB-serial adapter (CP210x, CH340,"
    Write-Host "  FTDI, 'USB Serial', 'Silicon Labs') via its friendly name. Pass -Port COMn to force one."
    Write-Host ""
    Write-Host "Baud:"
    Write-Host "  Default 115200 = the OTGW application baud, which is what the exception dump + stack"
    Write-Host "  trace print at. For the boot-ROM reset-cause line ('ets ... rst cause:N') pass -Baud 74880."
    Write-Host ""
    Write-Host "Important:"
    Write-Host "  This holds the COM port exclusively. OTmonitor and firmware flashing cannot share it"
    Write-Host "  while the capture runs. Press Q to stop cleanly (Ctrl+C also works)."
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "  .\scripts\capture-usb-serial.bat"
    Write-Host "  .\scripts\capture-usb-serial.bat -Port COM5"
    Write-Host "  .\scripts\capture-usb-serial.bat -Baud 74880 -DurationSeconds 120"
}

if ($Help) {
    Show-Help
    exit 0
}

# --- cooperative stop flag shared with the Ctrl+C handler (same pattern as
#     capture-mqtt-debug.bat) so Q and Ctrl+C both stop the read loop cleanly. ---
function Initialize-CancelFlag {
    if (-not ("OtgUsbCapture.CancelFlag" -as [type])) {
        Add-Type -TypeDefinition @"
using System;
using System.Threading;
namespace OtgUsbCapture
{
    public static class CancelFlag
    {
        private static int stopRequested;
        private static readonly ConsoleCancelEventHandler cancelHandler = OnCancelKeyPress;
        public static ConsoleCancelEventHandler Handler { get { return cancelHandler; } }
        public static bool StopRequested { get { return Interlocked.CompareExchange(ref stopRequested, 0, 0) != 0; } }
        public static void Reset() { Interlocked.Exchange(ref stopRequested, 0); }
        public static void RequestStop() { Interlocked.Exchange(ref stopRequested, 1); }
        private static void OnCancelKeyPress(object sender, ConsoleCancelEventArgs e) { e.Cancel = true; RequestStop(); }
    }
}
"@
    }
    [OtgUsbCapture.CancelFlag]::Reset()
}

function Test-StopRequested {
    if ([OtgUsbCapture.CancelFlag]::StopRequested) { return $true }
    try {
        if (-not [Console]::IsInputRedirected) {
            while ([Console]::KeyAvailable) {
                if ([Console]::ReadKey($true).Key -eq [ConsoleKey]::Q) {
                    [OtgUsbCapture.CancelFlag]::RequestStop()
                    return $true
                }
            }
        }
    } catch { }
    return [OtgUsbCapture.CancelFlag]::StopRequested
}

# --- Auto-detect a USB-serial adapter by friendly name. Returns "COMn" or $null. ---
function Find-UsbSerialPort {
    $namePattern = 'CP210|CH340|CH910|FT232|FTDI|USB.?SERIAL|USB Serial|Silicon Labs|Prolific|wch'
    try {
        $candidates = Get-CimInstance -ClassName Win32_PnPEntity -ErrorAction Stop |
            Where-Object { $_.Name -and $_.Name -match '\(COM\d+\)' -and $_.Name -match $namePattern }
        foreach ($c in $candidates) {
            if ($c.Name -match '\((COM\d+)\)') { return $Matches[1] }
        }
    } catch { }
    # Fallback: if exactly one serial port exists on the machine, use it.
    try {
        $ports = [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object -Unique
        if ($ports.Count -eq 1) { return $ports[0] }
    } catch { }
    return $null
}

function New-Utf8Writer {
    param([Parameter(Mandatory = $true)][string]$Path)
    $utf8NoBom = New-Object System.Text.UTF8Encoding -ArgumentList $false
    return New-Object System.IO.StreamWriter -ArgumentList $Path, $true, $utf8NoBom
}

# --- Resolve the port ---
if ([string]::IsNullOrWhiteSpace($Port)) {
    $Port = Find-UsbSerialPort
    if ([string]::IsNullOrWhiteSpace($Port)) {
        Write-Host "No USB-serial port found. Plug in the OTGW USB cable, or pass -Port COMn."
        Write-Host "Available ports: $(([System.IO.Ports.SerialPort]::GetPortNames() -join ', '))"
        exit 1
    }
    Write-Host "Auto-detected serial port: $Port"
} else {
    Write-Host "Using serial port: $Port"
}

# --- Prepare output ---
if ([System.IO.Path]::IsPathRooted($OutputRoot)) {
    $outputRootPath = [System.IO.Path]::GetFullPath($OutputRoot)
} else {
    $outputRootPath = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputRoot))
}
$runName = Get-Date -Format "yyyyMMdd-HHmmss"
$runPath = Join-Path $outputRootPath $runName
New-Item -Path $runPath -ItemType Directory -Force | Out-Null
$rawLog   = Join-Path $runPath "usb-serial.log"
$crashLog = Join-Path $runPath "crash-frames.log"

Write-Host "Capturing $Port @ ${Baud} baud into $runPath"
Write-Host "Press Q to stop cleanly. Crash frames are mirrored into crash-frames.log."
if ($Baud -ne 115200) { Write-Host "Note: 74880 = boot-ROM reset banner; 115200 = exception dump + stack trace." }

# Markers that identify an ESP8266 panic dump. The stack block is captured whole
# (from >>>stack>>> to <<<stack<<<); the single-line markers are flagged individually.
$crashLineRegex  = '(Fatal exception|Exception \(|epc1=|epc2=|epc3=|excvaddr=|depc=|rst cause|ets [A-Z]|wdt reset|Soft WDT|user code done|load 0x)'
$stackBeginRegex = '>>>stack>>>'
$stackEndRegex   = '<<<stack<<<'

Initialize-CancelFlag
$cancelHandler = [OtgUsbCapture.CancelFlag]::Handler
[System.Console]::add_CancelKeyPress($cancelHandler)

$deadline = if ($DurationSeconds -gt 0) { (Get-Date).AddSeconds($DurationSeconds) } else { [DateTime]::MaxValue }

$raw = New-Utf8Writer -Path $rawLog
$crash = New-Utf8Writer -Path $crashLog
$sp = $null
$lineBuf = New-Object System.Text.StringBuilder
$inStack = $false
$crashFrameCount = 0
$openWarned = $false

function Write-Both {
    param([string]$line)
    $ts = (Get-Date).ToString('HH:mm:ss.fff')
    $raw.WriteLine("$ts  $line")
    $raw.Flush()
    if ($script:inStack) {
        $crash.WriteLine("$ts  $line"); $crash.Flush()
        if ($line -match $stackEndRegex) { $script:inStack = $false }
        return
    }
    if ($line -match $stackBeginRegex) {
        $script:inStack = $true
        $script:crashFrameCount++
        $crash.WriteLine("--- crash frame #$script:crashFrameCount @ $ts ---"); $crash.Flush()
        $crash.WriteLine("$ts  $line"); $crash.Flush()
        Write-Host "  [crash] stack trace captured (frame #$script:crashFrameCount)"
        return
    }
    if ($line -match $crashLineRegex) {
        $crash.WriteLine("$ts  $line"); $crash.Flush()
        Write-Host "  [crash] $line"
    }
}

try {
    while (-not (Test-StopRequested) -and (Get-Date) -lt $deadline) {
        # (Re)open the port if needed — survives unplug/replug and port-in-use.
        if ($null -eq $sp -or -not $sp.IsOpen) {
            try {
                $sp = New-Object System.IO.Ports.SerialPort $Port, $Baud, ([System.IO.Ports.Parity]::None), 8, ([System.IO.Ports.StopBits]::One)
                $sp.ReadTimeout = 500
                $sp.NewLine = "`n"
                $sp.Open()
                $openWarned = $false
                Write-Host "Port $Port opened."
            } catch {
                if (-not $openWarned) {
                    Write-Host "Cannot open $Port ($($_.Exception.Message)). Retrying (is OTmonitor/flasher holding it?)..."
                    $openWarned = $true
                }
                Start-Sleep -Milliseconds 1000
                continue
            }
        }

        # Drain available bytes; split into lines for crash-marker detection.
        try {
            $chunk = $sp.ReadExisting()
        } catch [TimeoutException] {
            $chunk = ""
        } catch {
            Write-Host "Read error on $Port ($($_.Exception.Message)); reopening..."
            try { $sp.Close() } catch { }
            $sp = $null
            continue
        }

        if (-not [string]::IsNullOrEmpty($chunk)) {
            foreach ($ch in $chunk.ToCharArray()) {
                if ($ch -eq "`n") {
                    $line = $lineBuf.ToString().TrimEnd("`r")
                    [void]$lineBuf.Clear()
                    Write-Both -line $line
                } elseif ($ch -ne "`r") {
                    [void]$lineBuf.Append($ch)
                }
            }
        } else {
            Start-Sleep -Milliseconds 50
        }
    }

    # Flush any trailing partial line.
    if ($lineBuf.Length -gt 0) { Write-Both -line $lineBuf.ToString() }
}
finally {
    if ($sp) { try { if ($sp.IsOpen) { $sp.Close() } } catch { }; try { $sp.Dispose() } catch { } }
    try { $raw.Flush(); $raw.Dispose() } catch { }
    try { $crash.Flush(); $crash.Dispose() } catch { }
    if ($cancelHandler) { [System.Console]::remove_CancelKeyPress($cancelHandler) }
    Write-Host ""
    Write-Host "Stopped. Raw log:   $rawLog"
    Write-Host "Crash frames ($crashFrameCount): $crashLog"
    if ($crashFrameCount -gt 0) {
        Write-Host "Decode with: xtensa-lx106-elf-addr2line -e <firmware>.elf 0x<epc1>  (and 0x<excvaddr>)"
    }
}
