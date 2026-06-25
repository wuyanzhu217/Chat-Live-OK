#Requires -Version 5.1
<#
.SYNOPSIS
  Configure and build appccvWriter (native Windows host).

.EXAMPLE
  .\scripts\build.ps1
  .\scripts\build.ps1 -BuildDir build\release -Config Release
#>
param(
    [string]$BuildDir = "build\mingw-debug",
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Config = "Debug",
    [string]$QtRoot = $env:QT_ROOT,
    [string]$MingwRoot = $env:MINGW_ROOT,
    [string]$NinjaDir = $env:NINJA_DIR,
    [string]$CmakeDir = $env:CMAKE_DIR
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")

if (-not $QtRoot) {
    if ($env:QTDIR) {
        $QtRoot = $env:QTDIR
    } else {
        $QtRoot = "E:\QT\6.9.1\mingw_64"
    }
}

if (-not $MingwRoot) {
    $MingwRoot = "E:\QT\Tools\mingw1310_64"
}

if (-not $NinjaDir) {
    $candidate = "E:\QT\Tools\Ninja"
    if (Test-Path $candidate) {
        $NinjaDir = $candidate
    } else {
        $NinjaDir = "C:\Ninja"
    }
}

if (-not $CmakeDir) {
    $candidate = "D:\CMake\bin"
    if (Test-Path $candidate) {
        $CmakeDir = $candidate
    } else {
        $CmakeDir = "C:\CMake\bin"
    }
}

if (-not (Test-Path (Join-Path $QtRoot "lib\cmake\Qt6"))) {
    throw "Qt6 not found at QT_ROOT=$QtRoot. Set `$env:QT_ROOT or pass -QtRoot."
}

if (-not (Test-Path (Join-Path $MingwRoot "bin\g++.exe"))) {
    throw "MinGW not found at MINGW_ROOT=$MingwRoot. Set `$env:MINGW_ROOT or pass -MingwRoot."
}

$env:QT_ROOT = $QtRoot
$env:MINGW_ROOT = $MingwRoot
$env:QTDIR = $QtRoot
$env:PATH = "$NinjaDir;$CmakeDir;$MingwRoot\bin;$QtRoot\bin;$env:PATH"

$BuildPath = Join-Path $ProjectRoot $BuildDir
Write-Host "Project : $ProjectRoot"
Write-Host "Build   : $BuildPath ($Config)"
Write-Host "Qt      : $QtRoot"
Write-Host "MinGW   : $MingwRoot"

& cmake -S $ProjectRoot -B $BuildPath -G Ninja -DCMAKE_BUILD_TYPE=$Config
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& cmake --build $BuildPath
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$ExePath = Join-Path $BuildPath "appccvWriter.exe"
$WindeployQt = Join-Path $QtRoot "bin\windeployqt.exe"
if (Test-Path $WindeployQt) {
    & $WindeployQt $ExePath --no-translations
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} else {
    Write-Warning "windeployqt not found, skip Qt runtime deployment."
}

Write-Host "Build OK: $ExePath"
