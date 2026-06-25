#Requires -Version 5.1
<#
.SYNOPSIS
  Run appccvWriter on the Windows host (GUI + camera require host, not Docker).

.EXAMPLE
  .\scripts\run.ps1
  .\scripts\run.ps1 -BuildDir build\docker-debug
#>
param(
    [string]$BuildDir = "build\mingw-debug"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$ExePath = Join-Path $ProjectRoot (Join-Path $BuildDir "appccvWriter.exe")

if (-not (Test-Path $ExePath)) {
    throw "Executable not found: $ExePath`nRun .\scripts\build.ps1 first."
}

Write-Host "Starting: $ExePath"
& $ExePath
