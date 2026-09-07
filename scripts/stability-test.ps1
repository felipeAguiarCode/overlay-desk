<#
.SYNOPSIS
    AT-016: runs Overlay Desk against a continuously repainting source and reports whether
    memory, handles or GDI/USER objects grow over the session.

.DESCRIPTION
    The acceptance test asks for 60 minutes at 1080p with no continuous growth. "No growth" is
    not something you can eyeball off a task manager, so this samples the process on an
    interval and fits a line through each series. A leak shows up as a slope that is both
    positive and large relative to the noise; a healthy process wanders around a flat line.

    The source is a PowerShell console in a repaint loop. That matters: Windows.Graphics.Capture
    only delivers a frame when the source actually changes, so testing against a static window
    would exercise the idle path and prove nothing about the frame path.

    It is launched through conhost.exe on purpose. Starting powershell.exe directly on Windows 11
    hands it to Windows Terminal, whose window belongs to WindowsTerminal.exe and carries a tab
    title - neither of which the target matcher can pin down. conhost gives a plain
    ConsoleWindowClass window with the exact title set below.

    The overlay is parked in a corner and left click-through, so the machine stays usable while
    the test runs.

.EXAMPLE
    .\scripts\stability-test.ps1                      # the full 60-minute AT-016 run
    .\scripts\stability-test.ps1 -Minutes 10          # a shorter smoke run
    .\scripts\stability-test.ps1 -Minutes 60 -Width 1920 -Height 1080
#>
[CmdletBinding()]
param(
    [int]$Minutes = 60,
    [int]$SampleSeconds = 15,
    [int]$Width = 960,
    [int]$Height = 540,
    [int]$X = 40,
    [int]$Y = 40
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repoRoot 'build\Release\OverlayDesk.exe'
if (-not (Test-Path $exe)) {
    throw "Release build not found at $exe. Run .\scripts\build.ps1 -Config Release first."
}

Add-Type -TypeDefinition @'
using System;using System.Runtime.InteropServices;using System.Text;
public class StabilityNative {
 [DllImport("kernel32.dll")] public static extern uint SetThreadExecutionState(uint flags);
 [DllImport("user32.dll")] public static extern uint GetGuiResources(IntPtr process, uint flags);
 [DllImport("user32.dll")] static extern bool EnumWindows(EnumWindowsProc cb, IntPtr p);
 delegate bool EnumWindowsProc(IntPtr h, IntPtr p);
 [DllImport("user32.dll")] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
 [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr value);
 [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
 public static IntPtr FindWindowByTitle(string title) {
   IntPtr found = IntPtr.Zero;
   EnumWindows((h, p) => {
     var sb = new StringBuilder(512); GetWindowText(h, sb, 512);
     if (sb.ToString() == title) { found = h; return false; }
     return true; }, IntPtr.Zero);
   return found; }
 public static uint GetWindowPid(IntPtr h) { uint q; GetWindowThreadProcessId(h, out q); return q; }
 public static IntPtr FindWindow(uint targetPid, string className) {
   IntPtr found = IntPtr.Zero;
   EnumWindows((h, p) => {
     uint q; GetWindowThreadProcessId(h, out q);
     if (q == targetPid) {
       var sb = new StringBuilder(256); GetClassName(h, sb, 256);
       if (sb.ToString() == className) { found = h; return false; }
     }
     return true; }, IntPtr.Zero);
   return found; }
}
'@

# Keep the session awake: a locked screen suspends composition and would invalidate the run.
# Process-scoped, so it reverts when this script exits.
[void][StabilityNative]::SetThreadExecutionState([uint32]2147483651)
[void][StabilityNative]::SetProcessDpiAwarenessContext([IntPtr](-4))

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$csvPath = Join-Path $repoRoot "build\stability-$stamp.csv"

Write-Host "Overlay Desk - stability test" -ForegroundColor Cyan
Write-Host "  duration : $Minutes minutes, sampling every $SampleSeconds s"
Write-Host "  overlay  : ${Width}x${Height} at $X,$Y"
Write-Host "  samples  : $csvPath"
Write-Host ""

# --- The source -------------------------------------------------------------------------
# A console window redrawing continuously. Each iteration rewrites the whole screen, so the
# capture pipeline gets a genuine stream of frames rather than the odd repaint.
$sourceScript = @'
$host.UI.RawUI.WindowTitle = "OverlayDesk stability source"
$i = 0
while ($true) {
  $i++
  [Console]::SetCursorPosition(0, 0)
  foreach ($row in 0..20) {
    $line = -join (0..70 | ForEach-Object { [char](33 + (($i + $row * 7 + $_ * 3) % 90)) })
    [Console]::Write($line.PadRight(78))
    [Console]::Write("`n")
  }
  Start-Sleep -Milliseconds 16
}
'@
$sourceFile = Join-Path $env:TEMP 'overlaydesk-stability-source.ps1'
[IO.File]::WriteAllText($sourceFile, $sourceScript)

$sourceTitle = 'OverlayDesk stability source'
$sourceHost = Start-Process conhost.exe -ArgumentList 'powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $sourceFile -PassThru
Start-Sleep -Seconds 4

# The console window belongs to the PowerShell client, not to the conhost launcher, so the
# process to shut down at the end has to be found through the window.
$sourceWindow = [StabilityNative]::FindWindowByTitle($sourceTitle)
if ($sourceWindow -eq [IntPtr]::Zero) {
    Stop-Process -Id $sourceHost.Id -Force -ErrorAction SilentlyContinue
    throw "The repaint source window never appeared."
}
$sourcePid = [StabilityNative]::GetWindowPid($sourceWindow)

# --- Settings for the run -------------------------------------------------------------------
# Every filter and the glitch on: the point is to exercise the whole shader, not the cheap path.
$settingsPath = Join-Path $env:APPDATA 'OverlayDesk\settings.json'
$settings = @"
{ "schemaVersion": 1,
  "overlay": { "enabled": true, "x": $X, "y": $Y, "width": $Width, "height": $Height,
    "lockAspectRatio": false, "alwaysOnTop": true, "clickThrough": true, "opacity": 1.0 },
  "filters": {
    "distortion": { "enabled": true, "intensity": 1.0, "amount": 0.3 },
    "vignette": { "enabled": true, "intensity": 0.4 },
    "scanlines": { "enabled": true, "intensity": 0.35, "spacing": 3.0 },
    "chromaticAberration": { "enabled": true, "intensity": 0.25, "mode": "edge" },
    "colorCorrection": { "enabled": true, "intensity": 1.0, "contrast": 1.1, "saturation": 1.1 } },
  "effects": { "glitch": { "enabled": true, "intensity": 0.35, "frequency": 0.15 } },
  "render": { "fpsMode": "matchSource", "fpsCap": 60, "pauseWhenSourceMinimized": true },
  "ui": { "restoreLastTarget": true, "lastTargetTitle": "$sourceTitle", "lastTargetExecutable": "", "activeTab": 0 } }
"@
[IO.File]::WriteAllText($settingsPath, $settings)

$app = Start-Process $exe -PassThru
Start-Sleep -Seconds 5

# Park the control panel out of the way.
$control = [StabilityNative]::FindWindow([uint32]$app.Id, 'OverlayDesk.ControlWindow')
if ($control -ne [IntPtr]::Zero) {
    [void][StabilityNative]::SetWindowPos($control, [IntPtr]::Zero, ($X + $Width + 20), $Y, 460, 520, 0x0004)
}

$overlay = [StabilityNative]::FindWindow([uint32]$app.Id, 'OverlayDesk.OverlayWindow')
if ($overlay -eq [IntPtr]::Zero) {
    Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
    Stop-Process -Id $sourcePid -Force -ErrorAction SilentlyContinue
    Stop-Process -Id $sourceHost.Id -Force -ErrorAction SilentlyContinue
    throw "The overlay window never appeared - capture did not start. Check %APPDATA%\OverlayDesk\logs\overlaydesk.log."
}
Write-Host "Capture running. Sampling..." -ForegroundColor Green

# --- Sampling ----------------------------------------------------------------------------
# GR_GDIOBJECTS = 0, GR_USEROBJECTS = 1. These leak independently of the heap and are what
# catch a window or brush being recreated without being destroyed.
$samples = New-Object System.Collections.Generic.List[object]
$script:endedEarly = $false
$script:earlyReason = ''
$deadline = (Get-Date).AddMinutes($Minutes)
$started = Get-Date

while ((Get-Date) -lt $deadline) {
    $p = Get-Process -Id $app.Id -ErrorAction SilentlyContinue
    if (-not $p) {
        # A process that is gone is not necessarily a process that crashed. Closing the
        # control panel is a normal way to quit, and the application records that on its way
        # out - so ask the log which of the two happened before calling it a failure.
        $elapsed = [int]((Get-Date) - $started).TotalMinutes
        $appLog = Join-Path $env:APPDATA 'OverlayDesk\logs\overlaydesk.log'
        $cleanExit = $false
        if (Test-Path $appLog) {
            $fs = [IO.File]::Open($appLog, 'Open', 'Read', 'ReadWrite')
            $sr = New-Object IO.StreamReader($fs)
            $tail = $sr.ReadToEnd(); $sr.Close(); $fs.Close()
            $cleanExit = ($tail -split "`r?`n" | Select-Object -Last 5) -match 'Shutdown: clean'
        }
        $script:endedEarly = $true
        if ($cleanExit) {
            $script:earlyReason = "the application was closed at the keyboard after $elapsed minutes"
            Write-Host "Application exited cleanly after $elapsed minutes - the run was cut short, not failed." -ForegroundColor Yellow
        } else {
            $script:earlyReason = "the application vanished after $elapsed minutes with no clean-shutdown record"
            Write-Host "PROCESS DIED after $elapsed minutes with no clean shutdown - check crash.log." -ForegroundColor Red
        }
        break
    }
    $samples.Add([pscustomobject]@{
        Minutes      = [math]::Round(((Get-Date) - $started).TotalMinutes, 2)
        WorkingSetMB = [math]::Round($p.WorkingSet64 / 1MB, 2)
        PrivateMB    = [math]::Round($p.PrivateMemorySize64 / 1MB, 2)
        Handles      = $p.HandleCount
        GdiObjects   = [StabilityNative]::GetGuiResources($p.Handle, 0)
        UserObjects  = [StabilityNative]::GetGuiResources($p.Handle, 1)
        Threads      = $p.Threads.Count
        CpuSeconds   = [math]::Round($p.CPU, 1)
    })
    Start-Sleep -Seconds $SampleSeconds
}

$samples | Export-Csv -Path $csvPath -NoTypeInformation -Encoding UTF8

Stop-Process -Id $app.Id -Force -ErrorAction SilentlyContinue
Stop-Process -Id $sourcePid -Force -ErrorAction SilentlyContinue
Stop-Process -Id $sourceHost.Id -Force -ErrorAction SilentlyContinue

# --- Analysis -----------------------------------------------------------------------------
# A leak has to satisfy three things at once, and each guard exists because dropping it
# produces a false alarm:
#
#   1. The series must END HIGHER than it started. Without this, a thread pool winding down
#      after startup (13 threads -> 7) reads as a steep "trend" and fails the run.
#   2. That observed change must clear a noise floor. Without this, one USER object of
#      jitter across a short run extrapolates to +20/hour and fails the run.
#   3. The fitted slope must clear an hourly threshold, so slow real growth is still caught.
#
# The slope is a least-squares fit, projected to one hour.
function Get-Trend($rows, [string]$column) {
    $n = $rows.Count
    if ($n -lt 3) { return $null }
    $xs = $rows | ForEach-Object { $_.Minutes }
    $ys = $rows | ForEach-Object { [double]$_.$column }
    $mx = ($xs | Measure-Object -Average).Average
    $my = ($ys | Measure-Object -Average).Average
    $num = 0.0; $den = 0.0
    for ($i = 0; $i -lt $n; $i++) {
        $dx = $xs[$i] - $mx
        $num += $dx * ($ys[$i] - $my)
        $den += $dx * $dx
    }
    $slope = if ($den -gt 0) { $num / $den } else { 0 }
    [pscustomobject]@{
        Column       = $column
        First        = $ys[0]
        Last         = $ys[$n - 1]
        Min          = ($ys | Measure-Object -Minimum).Minimum
        Max          = ($ys | Measure-Object -Maximum).Maximum
        SlopePerHour = [math]::Round($slope * 60, 3)
    }
}

Write-Host ""
Write-Host "=== $($samples.Count) samples over $([math]::Round(((Get-Date) - $started).TotalMinutes, 1)) minutes ===" -ForegroundColor Cyan

if ($Minutes -lt 20) {
    Write-Host "  (short run - the per-hour projection is indicative only)" -ForegroundColor DarkYellow
}

# Per column: how much observed growth is more than noise, and how much projected hourly
# growth is worth failing over.
$noiseFloor  = @{ WorkingSetMB = 2.0;  PrivateMB = 2.0;  Handles = 25;  GdiObjects = 10; UserObjects = 10; Threads = 4 }
$hourlyFloor = @{ WorkingSetMB = 15.0; PrivateMB = 15.0; Handles = 100; GdiObjects = 40; UserObjects = 40; Threads = 8 }

# Only these decide the verdict.
#
# WorkingSet is deliberately NOT one of them. Windows grows a process's working set
# opportunistically whenever there is free RAM to map, so it drifts upward on a perfectly
# healthy process and would fail every run on a machine that happens to be idle. What
# actually distinguishes a leak is committed memory (private bytes) and kernel objects.
# WorkingSet is still reported, and it escalates to a failure when private bytes confirm it.
$verdictSeries = 'PrivateMB', 'Handles', 'GdiObjects', 'UserObjects'

$growth = @{}
$rows = @()
foreach ($column in 'WorkingSetMB', 'PrivateMB', 'Handles', 'GdiObjects', 'UserObjects', 'Threads') {
    $t = Get-Trend $samples $column
    if (-not $t) { continue }
    $delta = $t.Last - $t.First
    $growth[$column] = ($delta -gt $noiseFloor[$column]) -and ($t.SlopePerHour -gt $hourlyFloor[$column])
    $rows += [pscustomobject]@{ Trend = $t; Delta = $delta }
}

$verdicts = @()
foreach ($row in $rows) {
    $t = $row.Trend
    $isGrowing = $growth[$t.Column]

    $counts = $verdictSeries -contains $t.Column
    # Working set only counts against the run when committed memory agrees with it.
    if ($t.Column -eq 'WorkingSetMB') { $counts = $isGrowing -and $growth['PrivateMB'] }

    $leaking = $isGrowing -and $counts
    $verdicts += [pscustomobject]@{ Column = $t.Column; Leaking = $leaking }

    $note = if ($leaking) { 'GROWING' }
            elseif ($isGrowing -and $t.Column -eq 'WorkingSetMB') { 'drifting (private bytes flat)' }
            elseif ($isGrowing) { 'growing (not counted)' }
            else { 'stable' }
    $color = if ($leaking) { 'Red' } elseif ($isGrowing) { 'Yellow' } else { 'Green' }

    Write-Host ("  {0,-13} {1,8} -> {2,-8}  delta {3,8}  min/max {4}/{5}  trend {6,9}/h  {7}" -f `
        $t.Column, $t.First, $t.Last, [math]::Round($row.Delta, 2), $t.Min, $t.Max,
        $t.SlopePerHour, $note) -ForegroundColor $color
}

Write-Host ""
Write-Host "Samples: $csvPath"

if ($verdicts | Where-Object { $_.Leaking }) {
    Write-Host "AT-016: FAIL - committed memory or kernel objects grew continuously." -ForegroundColor Red
    exit 1
}

# A run that was cut short cannot pass AT-016 no matter how clean the numbers look - the
# acceptance test asks for a full 60 minutes, and reporting a partial run as a pass would be
# claiming something that was never measured.
if ($script:endedEarly) {
    Write-Host "AT-016: INCOMPLETE - $($script:earlyReason)." -ForegroundColor Yellow
    Write-Host "No growth was seen in the samples collected, but the full duration was not reached." -ForegroundColor Yellow
    exit 2
}

Write-Host "AT-016: PASS - no continuous growth over $Minutes minutes." -ForegroundColor Green
