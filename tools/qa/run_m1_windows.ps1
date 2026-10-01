[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$SourceRoot,
    [Parameter(Mandatory = $true)][string]$BuildRoot,
    [Parameter(Mandatory = $true)][string]$Rom,
    [Parameter(Mandatory = $true)][string]$EvidenceRoot,
    [string]$RunId = ('windows-' + (Get-Date).ToUniversalTime().ToString('yyyyMMddTHHmmssZ'))
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class TetrisphereNativeInput {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, out RECT rect);
    [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint mapType);
    [DllImport("user32.dll")] public static extern void keybd_event(byte key, byte scan, uint flags, UIntPtr extra);
}
'@

function Require-File([string]$Path, [string]$Name) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "$Name is missing: $Path" }
}

function Digest([string]$Path) {
    return (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant()
}

function Press-Key([byte]$VirtualKey, [int]$HoldMilliseconds = 160) {
    $scan = [byte][TetrisphereNativeInput]::MapVirtualKey($VirtualKey, 0)
    $extended = if ($VirtualKey -ge 0x25 -and $VirtualKey -le 0x28) { 1 } else { 0 }
    [TetrisphereNativeInput]::keybd_event($VirtualKey, $scan, $extended, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds $HoldMilliseconds
    [TetrisphereNativeInput]::keybd_event($VirtualKey, $scan, ($extended -bor 2), [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 180
}

function Capture-Window([IntPtr]$Window, [string]$Path) {
    $rectangle = New-Object TetrisphereNativeInput+RECT
    if (-not [TetrisphereNativeInput]::GetWindowRect($Window, [ref]$rectangle)) {
        throw 'GetWindowRect failed for the native RT64 window'
    }
    $width = $rectangle.Right - $rectangle.Left
    $height = $rectangle.Bottom - $rectangle.Top
    if ($width -lt 320 -or $height -lt 240) { throw 'native RT64 window has invalid bounds' }
    $bitmap = New-Object System.Drawing.Bitmap $width, $height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen($rectangle.Left, $rectangle.Top, 0, 0,
            (New-Object System.Drawing.Size $width, $height))
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

$SourceRoot = (Resolve-Path -LiteralPath $SourceRoot).Path
$BuildRoot = (Resolve-Path -LiteralPath $BuildRoot).Path
$Rom = (Resolve-Path -LiteralPath $Rom).Path
New-Item -ItemType Directory -Force -Path $EvidenceRoot | Out-Null
$EvidenceRoot = (Resolve-Path -LiteralPath $EvidenceRoot).Path
$receiptPath = Join-Path $BuildRoot 'windows-build-receipt.json'
$binary = Join-Path $BuildRoot 'Release\tetrisphere-m1.exe'
$rt64Receipt = Join-Path $BuildRoot 'rt64-verification.json'
Require-File $receiptPath 'windows-build-receipt.json'
Require-File $binary 'native Windows executable'
Require-File $rt64Receipt 'RT64 verification receipt'
Require-File $Rom 'authorized ROM'
$receipt = Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
if ($receipt.platform -ne 'windows-x86_64' -or
    $receipt.binary_sha256 -ne (Digest $binary)) {
    throw 'native Windows binary does not match windows-build-receipt.json'
}
foreach ($runtimeName in @('SDL2.dll', 'dxcompiler.dll', 'dxil.dll')) {
    $runtimePath = Join-Path (Split-Path $binary) $runtimeName
    Require-File $runtimePath "Windows runtime $runtimeName"
    $recordedRuntimeHash = $receipt.runtime_sha256.PSObject.Properties[$runtimeName].Value
    if ($recordedRuntimeHash -ne (Digest $runtimePath)) {
        throw "Windows runtime $runtimeName does not match windows-build-receipt.json"
    }
}

$stdout = Join-Path $EvidenceRoot "$RunId.out"
$stderr = Join-Path $EvidenceRoot "$RunId.err"
$audio = Join-Path $EvidenceRoot "$RunId.wav"
$before = Join-Path $EvidenceRoot "$RunId-before.png"
$after = Join-Path $EvidenceRoot "$RunId-after.png"
$runReceipt = Join-Path $EvidenceRoot 'native-windows-run.json'
$env:TETRISPHERE_AUDIO_CAPTURE_WAV = $audio
$env:TETRISPHERE_CONTROLLER_FAMILY = 'keyboard'
$env:TETRISPHERE_RUN_ID = $RunId

$process = Start-Process -FilePath $binary -ArgumentList ('"{0}"' -f $Rom) `
    -WorkingDirectory (Split-Path $binary) -RedirectStandardOutput $stdout `
    -RedirectStandardError $stderr -PassThru
try {
    $deadline = (Get-Date).AddSeconds(45)
    do {
        Start-Sleep -Milliseconds 250
        $process.Refresh()
        $ready = $process.MainWindowHandle -ne [IntPtr]::Zero -and
            (Test-Path -LiteralPath $stdout) -and
            ((Get-Content -LiteralPath $stdout -Raw -ErrorAction SilentlyContinue) -match
                '"event":"rt64_device_ready"')
    } while (-not $ready -and -not $process.HasExited -and (Get-Date) -lt $deadline)
    if (-not $ready) { throw 'native RT64 window did not become ready in the interactive session' }
    [TetrisphereNativeInput]::SetForegroundWindow($process.MainWindowHandle) | Out-Null
    # On the Windows reference PC the unskippable startup/cinematic does not
    # begin polling controller state until roughly 40 seconds after RT64 setup.
    Start-Sleep -Seconds 45

    # Acceptance route observed on this build: title/profile, then the
    # interactive Training sphere, where the final inputs rotate/move pieces
    # and advance tutorial state.
    foreach ($step in @(
        @(0x0D, 1800), @(0x0D, 1800), @(0x5A, 1800), @(0x5A, 1800),
        @(0x58, 1300), @(0x5A, 1800), @(0x58, 1300), @(0x28, 900),
        @(0x5A, 8000)
    )) {
        Press-Key ([byte]$step[0])
        Start-Sleep -Milliseconds $step[1]
    }
    Capture-Window $process.MainWindowHandle $before
    foreach ($key in @(0x25, 0x25, 0x25, 0x25, 0x25, 0x5A, 0x5A, 0x5A, 0x26, 0x28)) {
        Press-Key ([byte]$key)
        Start-Sleep -Milliseconds 450
    }
    Start-Sleep -Seconds 2
    Capture-Window $process.MainWindowHandle $after
    Start-Sleep -Seconds 2
} finally {
    if (-not $process.HasExited) {
        $process.CloseMainWindow() | Out-Null
        if (-not $process.WaitForExit(5000)) { Stop-Process -Id $process.Id -Force }
    }
}

foreach ($artifact in @($stdout, $audio, $before, $after)) {
    Require-File $artifact 'native Windows evidence artifact'
}
$log = Get-Content -LiteralPath $stdout -Raw
$buildMatch = [regex]::Match($log, '"event":"diagnostic_start","build_id":"([0-9a-f]{64})"')
if (-not $buildMatch.Success -or $log -notmatch '"event":"native_windows_started"' -or
    $log -notmatch '"event":"rt64_present"' -or
    $log -notmatch '"event":"logical_input_sample"') {
    throw 'native Windows event log is incomplete'
}
$result = [ordered]@{
    schema_version = 1
    run_id = $RunId
    platform = 'windows-x86_64'
    native = $true
    emulator = $false
    compatibility_layer = $false
    source_commit = $receipt.source_commit
    build_id = $buildMatch.Groups[1].Value
    binary_sha256 = Digest $binary
    rt64_receipt_sha256 = Digest $rt64Receipt
    capture_source = 'PCM accepted by the opened SDL playback device'
    session_id = [System.Diagnostics.Process]::GetCurrentProcess().SessionId
    artifacts = [ordered]@{
        screenshot_before = @{path=(Split-Path $before -Leaf); sha256=Digest $before}
        screenshot = @{path=(Split-Path $after -Leaf); sha256=Digest $after}
        audio = @{path=(Split-Path $audio -Leaf); sha256=Digest $audio}
        event_log = @{path=(Split-Path $stdout -Leaf); sha256=Digest $stdout}
        stderr = @{path=(Split-Path $stderr -Leaf); sha256=Digest $stderr}
    }
}
$result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $runReceipt -Encoding UTF8
$result | ConvertTo-Json -Depth 6
