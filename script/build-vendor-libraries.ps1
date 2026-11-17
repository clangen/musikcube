[CmdletBinding()]
param(
    [ValidateSet('All','curl','openssl','lame','libopenmpt','ffmpeg','static')]
    [string]$Library = 'All',
    [string]$MsysRoot,
    [string]$FFmpegInstall,
    [switch]$Preflight,
    [string]$AuditDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$triplet = 'musikcube-x64'
$vcpkgTag = '2025.10.17'
$ffmpegTag = 'n8.0'

function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Program exited with code $LASTEXITCODE."
    }
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio x64 C++ Build Tools are required.' }
$toolsVersion = (Get-Content (Join-Path $vs 'VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt') -Raw).Trim()
$nativeTools = Join-Path $vs "VC\Tools\MSVC\$toolsVersion\bin\Hostx64\x64"
$cmakeTools = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
$dumpbin = Join-Path $nativeTools 'dumpbin.exe'

function Test-Bundle {
    param([string]$Directory)
    $binaries = @(Get-ChildItem -LiteralPath $Directory -Recurse -File | Where-Object Extension -In '.dll','.exe')
    if (!$binaries.Count) { throw "No Windows binaries in $Directory." }
    $failures = foreach ($file in $binaries) {
        $headers = Invoke-Checked $dumpbin @('/headers', $file.FullName)
        if (!($headers -match '8664 machine')) { "$($file.Name): expected x64" }
        $imports = Invoke-Checked $dumpbin @('/dependents', $file.FullName)
        $forbidden = $imports | Select-String '(?i)^\s*((?:msys-|cyg|libgcc|libstdc\+\+|libwinpthread|libssp|libunwind|libc\+\+|vcruntime|msvcp|msvcr\d|concrt|ucrtbase|api-ms-win-crt)[\w+.-]*\.dll)\s*$'
        foreach ($entry in $forbidden) { "$($file.Name): $($entry.Matches[0].Groups[1].Value)" }
    }
    if ($failures) { throw ("Bundle failed its architecture or runtime check:" + [Environment]::NewLine + ($failures -join [Environment]::NewLine)) }
    Write-Host "Checked $($binaries.Count) x64 binaries in $Directory."
}

if ($AuditDirectory) {
    Test-Bundle $AuditDirectory
    return
}

$vendor = Join-Path (Split-Path $PSScriptRoot) 'vendor'
$support = Join-Path $PSScriptRoot 'windows-vendor'
$work = Join-Path $vendor '.build'
$msysVersion = '2025-12-13'
$portableMsys = !$MsysRoot
if ($portableMsys) {
    $MsysRoot = Join-Path $work "msys2-$msysVersion\msys64"
} else {
    $MsysRoot = (Resolve-Path -LiteralPath $MsysRoot).Path
}

function Get-MsysBuildTools {
    $tools = 'bash.exe','make.exe','pkg-config.exe'
    $missing = @($tools | Where-Object { !(Test-Path (Join-Path $MsysRoot "usr\bin\$_")) })
    if (!$missing.Count) { return }
    if (!$portableMsys) {
        throw "MSYS2 build tools missing under ${MsysRoot}: $($missing -join ', ')."
    }

    $bashPath = Join-Path $MsysRoot 'usr\bin\bash.exe'
    if (!(Test-Path $bashPath)) {
        $archiveName = "msys2-base-x86_64-$($msysVersion.Replace('-','')).tar.xz"
        $release = "https://github.com/msys2/msys2-installer/releases/download/$msysVersion"
        $downloads = Join-Path $work 'downloads'
        New-Item -ItemType Directory -Force -Path $downloads | Out-Null
        $archive = Join-Path $downloads $archiveName
        $checksum = "$archive.sha256"
        $curl = Join-Path $env:SystemRoot 'System32\curl.exe'
        if (!(Test-Path $checksum)) {
            Invoke-Checked $curl @('--fail','--location','--output',"$checksum.part","$release/$archiveName.sha256")
            $digest = (Get-Content -LiteralPath "$checksum.part" -Raw).Trim() -split '\s+' | Select-Object -First 1
            if ($digest -notmatch '^[0-9a-fA-F]{64}$') { throw 'Invalid MSYS2 release checksum.' }
            Move-Item -LiteralPath "$checksum.part" -Destination $checksum -Force
        }
        $expected = (Get-Content -LiteralPath $checksum -Raw).Trim() -split '\s+' | Select-Object -First 1
        if ($expected -notmatch '^[0-9a-fA-F]{64}$') { throw 'Invalid cached MSYS2 release checksum.' }
        if (!(Test-Path $archive)) {
            Invoke-Checked $curl @('--fail','--location','--output',"$archive.part","$release/$archiveName")
            if ((Get-FileHash -LiteralPath "$archive.part").Hash -ne $expected) {
                throw 'Downloaded MSYS2 archive failed the SHA256 check.'
            }
            Move-Item -LiteralPath "$archive.part" -Destination $archive -Force
        }
        if ((Get-FileHash -LiteralPath $archive).Hash -ne $expected) {
            throw "Cached MSYS2 archive failed the SHA256 check: $archive"
        }
        $destination = Split-Path $MsysRoot
        $extraction = Join-Path $work ("msys2-extract-" + [guid]::NewGuid())
        New-Item -ItemType Directory -Path $extraction | Out-Null
        Invoke-Checked (Join-Path $env:SystemRoot 'System32\tar.exe') @('-xf',$archive,'-C',$extraction)
        if (!(Test-Path (Join-Path $extraction 'msys64\usr\bin\bash.exe'))) {
            throw "Unexpected layout in $archive."
        }
        Move-Item -LiteralPath $extraction -Destination $destination
    }

    $env:MSYSTEM = 'MSYS'
    $env:CHERE_INVOKING = '1'
    $env:MSYS2_PATH_TYPE = 'minimal'
    Invoke-Checked $bashPath @('--login','-c','trap ''gpgconf --homedir /etc/pacman.d/gnupg --kill all'' EXIT; pacman -Sy --noconfirm --needed msys2-keyring && pacman -S --noconfirm --needed make pkgconf diffutils')
    foreach ($tool in $tools) {
        if (!(Test-Path (Join-Path $MsysRoot "usr\bin\$tool"))) {
            throw "Portable MSYS2 did not provide $tool."
        }
    }
}

$needsFFmpeg = $Library -in 'All','ffmpeg'
$git = (Get-Command git.exe -ErrorAction Stop).Source

if ($Preflight) {
    Write-Host "Visual Studio x64 tools: $nativeTools"
    Write-Host "Git: $git"
    if ($needsFFmpeg) {
        if (Test-Path (Join-Path $MsysRoot 'usr\bin\make.exe')) {
            Write-Host "MSYS2 build tools: $MsysRoot"
        } elseif ($portableMsys) {
            Write-Host "MSYS2 build tools will be provisioned under $MsysRoot."
        } else {
            throw "MSYS2 build tools missing under $MsysRoot."
        }
    }
    return
}
if ($needsFFmpeg -and !$FFmpegInstall) { Get-MsysBuildTools }
$bash = Join-Path $MsysRoot 'usr\bin\bash.exe'

$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
Push-Location (Split-Path $vcvars)
try {
    $environment = Invoke-Checked $env:ComSpec @('/d','/c','call vcvars64.bat >nul && set')
} finally {
    Pop-Location
}
foreach ($line in $environment) {
    if ($line -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
    }
}

New-Item -ItemType Directory -Force -Path $work | Out-Null
$vcpkgRoot = Join-Path $work "vcpkg-$vcpkgTag"
if (!(Test-Path (Join-Path $vcpkgRoot '.git'))) {
    Invoke-Checked $git @('clone','--depth','1','--branch',$vcpkgTag,'https://github.com/microsoft/vcpkg.git',$vcpkgRoot)
}
$vcpkg = Join-Path $vcpkgRoot 'vcpkg.exe'
if (!(Test-Path $vcpkg)) {
    Invoke-Checked (Join-Path $vcpkgRoot 'bootstrap-vcpkg.bat') @('-disableMetrics')
}
$packages = [ordered]@{
    curl = 'curl[core,ssl]'
    openssl = 'openssl'
    lame = 'mp3lame'
    libopenmpt = 'libopenmpt'
}
$staticPackages = @('libgme','taglib','zlib','libmicrohttpd')
$requested = if ($Library -eq 'All') { @($packages.Values) + $staticPackages } elseif ($Library -eq 'static') { $staticPackages } elseif ($Library -ne 'ffmpeg') { @($packages[$Library]) } else { @() }
if ($needsFFmpeg -and !$FFmpegInstall) { $requested = @($requested) + @('opus','libvorbis') }
if (Test-Path (Join-Path $cmakeTools 'cmake.exe')) { $env:PATH = "$cmakeTools;$env:PATH" }
$env:VCPKG_DISABLE_METRICS = '1'
$env:VCPKG_DEFAULT_BINARY_CACHE = Join-Path $work 'binary-cache'
New-Item -ItemType Directory -Force -Path $env:VCPKG_DEFAULT_BINARY_CACHE | Out-Null
if (@($requested).Count) {
    Push-Location $vcpkgRoot
    try {
        Invoke-Checked $vcpkg (@('install','--classic',"--triplet=$triplet",'--host-triplet=x64-windows',"--vcpkg-root=$vcpkgRoot","--overlay-triplets=$support\triplets") + @($requested))
    } finally {
        Pop-Location
    }
}
$installed = Join-Path $vcpkgRoot "installed\$triplet"
$stage = Join-Path $work ('stage-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-ffff'))
$lib = Join-Path $stage 'win64\lib'
$headers = Join-Path $stage 'include'
$dll = Join-Path $stage 'win64\dll'
New-Item -ItemType Directory -Force -Path $lib,$headers,$dll | Out-Null

function Publish-Library {
    param([string]$Group, [string]$Prefix, [string]$Name, [string[]]$Aliases)
    $candidates = @($Name) + $Aliases
    $importLibrary = foreach ($candidate in $candidates) {
        foreach ($directory in 'lib','bin') {
            $path = Join-Path $Prefix "$directory\$candidate.lib"
            if (Test-Path -LiteralPath $path) { $path; break }
        }
        if ($path -and (Test-Path -LiteralPath $path)) { break }
    }
    if (!$importLibrary) { throw "Missing import library for $Name under $Prefix." }
    $targets = @(Invoke-Checked $dumpbin @('/headers', $importLibrary) |
        Select-String '(?i)DLL name\s*:\s*(\S+\.dll)' |
        ForEach-Object { $_.Matches[0].Groups[1].Value } | Sort-Object -Unique)
    if ($targets.Count -ne 1) { throw "Could not identify exactly one DLL in $importLibrary." }
    $groupPath = Join-Path $dll $Group
    New-Item -ItemType Directory -Force -Path $groupPath | Out-Null
    Copy-Item -LiteralPath (Join-Path $Prefix "bin\$($targets[0])") -Destination $groupPath
    Copy-Item -LiteralPath $importLibrary -Destination (Join-Path $lib "$Name.lib")
}

if ($Library -in 'All','curl') { Publish-Library libcurl $installed libcurl @('libcurl_imp','curl') }
if ($Library -in 'All','openssl') {
    Publish-Library openssl $installed libssl @('ssl')
    Publish-Library openssl $installed libcrypto @('crypto')
}
if ($Library -in 'All','lame') {
    Publish-Library lame $installed libmp3lame @('mp3lame','lame','lame_enc')
    Copy-Item (Join-Path $lib 'libmp3lame.lib') (Join-Path $lib 'libmp3lameD.lib')
}
if ($Library -in 'All','libopenmpt') { Publish-Library libopenmpt $installed libopenmpt @('openmpt') }

if (@($requested).Count) { Copy-Item (Join-Path $installed 'include\*') -Destination $headers -Recurse -Force }
if ($needsFFmpeg) {
    if ($FFmpegInstall) {
        $ffInstall = (Resolve-Path -LiteralPath $FFmpegInstall).Path
    } else {
        $ffSource = Join-Path $work "ffmpeg-$ffmpegTag"
        if (!(Test-Path (Join-Path $ffSource '.git'))) {
            Invoke-Checked $git @('clone','--depth','1','--branch',$ffmpegTag,'https://github.com/FFmpeg/FFmpeg.git',$ffSource)
        }
        $ffBuild = Join-Path $work ("ffmpeg-" + (Split-Path $stage -Leaf))
        $ffInstall = Join-Path $ffBuild 'install'
        New-Item -ItemType Directory -Path $ffBuild,$ffInstall | Out-Null
        $env:PATH = (Join-Path $MsysRoot 'usr\bin') + ';' + $env:PATH
        Invoke-Checked $bash @((Join-Path $support 'build-ffmpeg.sh'),$ffSource,(Join-Path $support 'ffmpeg-options.txt'),$ffInstall,$installed,$ffBuild,$nativeTools)
    }
    foreach ($component in 'avcodec','avformat','avutil','swresample') {
        Publish-Library ffmpeg $ffInstall "$component-musikcube" @($component)
    }
    Copy-Item (Join-Path $ffInstall 'include\*') -Destination $headers -Recurse -Force
}
if ($Library -in 'All','static') {
    $archives = [ordered]@{
        'gme.lib' = @('lib/gme.lib')
        'gmeD.lib' = @('debug/lib/gme.lib','debug/lib/gmed.lib')
        'tag.lib' = @('lib/tag.lib')
        'tagD.lib' = @('debug/lib/tag.lib','debug/lib/tagd.lib')
        'zlibstatic.lib' = @('lib/zlib.lib','lib/zlibstatic.lib')
        'zlibstaticd.lib' = @('debug/lib/zlibd.lib','debug/lib/zlib.lib','debug/lib/zlibstaticd.lib')
        'libmicrohttpd.lib' = @('lib/libmicrohttpd.lib','lib/microhttpd.lib')
        'libmicrohttpdD.lib' = @('debug/lib/libmicrohttpd_d.lib','debug/lib/libmicrohttpd.lib','debug/lib/libmicrohttpdd.lib','debug/lib/microhttpd.lib')
    }
    foreach ($name in $archives.Keys) {
        $source = $archives[$name] | ForEach-Object { Join-Path $installed $_ } |
            Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
        if (!$source) { throw "Missing static archive for $name in $installed." }
        Copy-Item -LiteralPath $source -Destination (Join-Path $lib $name)
    }
}

if ($Library -in 'All','libopenmpt') {
    $openmptHeader = Join-Path $headers 'libopenmpt/libopenmpt_config.h'
    $content = [IO.File]::ReadAllText($openmptHeader)
    # The plugin uses the C API. Keep the header's C++ import suppression in place.
    $guard = '(?m)^#error "C\+\+ interface is disabled if libopenmpt is built as a DLL and the runtime is statically linked\.[^\r\n]*"'
    [IO.File]::WriteAllText($openmptHeader, ($content -replace $guard, '/* Allow the C API in C++ translation units. */'))
}

if ($Library -ne 'static') { Test-Bundle $dll }
New-Item -ItemType Directory -Force -Path "$vendor/include","$vendor/win64/lib","$vendor/win64/dll" | Out-Null
Copy-Item "$headers/*" "$vendor/include" -Recurse -Force
Copy-Item "$lib/*.lib" "$vendor/win64/lib" -Force
foreach ($group in Get-ChildItem -LiteralPath $dll -Directory) {
    $target = Join-Path "$vendor/win64/dll" $group.Name
    New-Item -ItemType Directory -Force -Path $target | Out-Null
    $expected = @(Get-ChildItem -LiteralPath $group.FullName -File -Filter *.dll)
    $expected | Copy-Item -Destination $target -Force
    Get-ChildItem -LiteralPath $target -File -Filter *.dll |
        Where-Object Name -NotIn $expected.Name | Remove-Item
}
Write-Host "Windows vendor libraries staged under $vendor."
