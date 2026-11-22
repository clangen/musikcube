# Record the desktop client area of a foreground musikcube GUI window.
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [int]$TargetProcessId,
    [string]$OutputDirectory,
    [ValidateRange(8, 1200)]
    [int]$Frames = 48,
    [ValidateRange(1, 1000)]
    [int]$IntervalMs = 25,
    [ValidateRange(0, 4096)]
    [int]$ShrinkPixels = 0,
    [ValidateRange(0, 30)]
    [int]$DelaySeconds = 3
)

$ErrorActionPreference = 'Stop'
$target = Get-Process -Id $TargetProcessId
if ($target.ProcessName -ne 'musikcube' -or $target.MainWindowHandle -eq [IntPtr]::Zero) {
    throw "Process $TargetProcessId must be a running musikcube GUI with an open window."
}

if (-not $OutputDirectory) {
    $buildRoot = Join-Path $PSScriptRoot '../../../build'
    $OutputDirectory = Join-Path $buildRoot ('ui/windows-window-' + [Guid]::NewGuid().ToString('N'))
}
$OutputDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
if (Test-Path -LiteralPath $OutputDirectory) {
    throw "Capture directory already exists. Choose a new directory."
}

Add-Type -Path (Join-Path $PSScriptRoot 'windows-window.cs') -ReferencedAssemblies System.Drawing
[void][System.IO.Directory]::CreateDirectory($OutputDirectory)
Write-Host "Bring musikcube to the foreground. Capturing in $DelaySeconds seconds."
if ($DelaySeconds -gt 0) {
    Start-Sleep -Seconds $DelaySeconds
}

try {
    $captured = [WindowsWindowCapture]::Record(
        $target.MainWindowHandle, $OutputDirectory, $Frames, $IntervalMs, $ShrinkPixels)
    Write-Host "Captured $captured frames in $OutputDirectory"
}
catch {
    Write-Host "Capture artifacts at $OutputDirectory"
    throw
}
