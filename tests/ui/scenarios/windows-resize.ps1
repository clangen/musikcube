# Exercise an open Windows GUI and retain frames for analysis.
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [int]$TargetProcessId,
    [ValidateRange(32, 4096)]
    [int]$ShrinkPixels = 120,
    [string]$OutputDirectory,
    [ValidateRange(0, 30)]
    [int]$DelaySeconds = 3
)

$ErrorActionPreference = 'Stop'
if (-not $OutputDirectory) {
    $buildRoot = Join-Path $PSScriptRoot '../../../build'
    $OutputDirectory = Join-Path $buildRoot ('ui/windows-resize-' + [Guid]::NewGuid().ToString('N'))
}

$capture = Join-Path $PSScriptRoot '../support/capture-windows-window.ps1'
& $capture -TargetProcessId $TargetProcessId -OutputDirectory $OutputDirectory -Frames 48 `
    -ShrinkPixels $ShrinkPixels -DelaySeconds $DelaySeconds

$frames = @(Import-Csv -LiteralPath (Join-Path $OutputDirectory 'frames.csv'))
if ($frames.Count -ne 48) {
    throw "Expected 48 captured frames, got $($frames.Count)."
}

$initial = $frames[0]
$settled = $frames[-1]
$resized = $frames | Where-Object {
    $_.phase -eq 'resize' -and [int]$_.width -lt [int]$initial.width -and [int]$_.height -lt [int]$initial.height
} | Select-Object -First 1
if (-not $resized) {
    throw 'The window never shrank on both axes. Start above the minimum window size.'
}
if ($settled.width -ne $initial.width -or $settled.height -ne $initial.height) {
    throw "Final client size did not return to $($initial.width)x$($initial.height)."
}

Write-Host "Resize verified. Inspect frames and timing in $OutputDirectory"
