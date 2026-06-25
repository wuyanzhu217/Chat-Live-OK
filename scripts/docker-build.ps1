#Requires -Version 5.1
<#
.SYNOPSIS
  Build appccvWriter inside a Windows Docker container (Qt mounted from host).

.EXAMPLE
  .\scripts\docker-build.ps1
  .\scripts\docker-build.ps1 -HostQtPath E:\QT -Config Release
#>
param(
    [string]$HostQtPath = $env:HOST_QT_PATH,
    [string]$BuildDir = "build\docker-debug",
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Config = "Debug",
    [switch]$BuildImage,
    [switch]$CiImage
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$DockerDir = Join-Path $ProjectRoot "docker"

if (-not $HostQtPath) {
    $HostQtPath = "E:\QT"
}

if (-not (Test-Path $HostQtPath)) {
    throw "HOST_QT_PATH not found: $HostQtPath. Install Qt or set -HostQtPath / `$env:HOST_QT_PATH."
}

$dockerInfo = docker info --format "{{.OSType}}" 2>$null
if ($LASTEXITCODE -ne 0 -or $dockerInfo -ne "windows") {
    throw @"
Docker is not running in Windows container mode (OSType=$dockerInfo).
Switch Docker Desktop to 'Windows containers' and retry.
"@
}

$ImageTag = if ($CiImage) { "chat-live-ok:ci" } else { "chat-live-ok:build" }
$Dockerfile = if ($CiImage) { "Dockerfile.ci" } else { "Dockerfile.build" }

if ($BuildImage) {
    Write-Host "Building image $ImageTag from docker\$Dockerfile ..."
    docker build -f (Join-Path $DockerDir $Dockerfile) -t $ImageTag $ProjectRoot
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$HostQtPathDocker = ($HostQtPath -replace '\\', '/')
$ProjectRootDocker = ($ProjectRoot.ToString() -replace '\\', '/')

$BuildCmd = @"
`$ErrorActionPreference = 'Stop'
`$env:QT_ROOT = 'C:/Qt/6.9.1/mingw_64'
`$env:MINGW_ROOT = 'C:/Qt/Tools/mingw1310_64'
`$env:QTDIR = `$env:QT_ROOT
`$env:PATH = 'C:/Ninja;C:/CMake/bin;' + `$env:MINGW_ROOT + '/bin;' + `$env:QT_ROOT + '/bin;' + `$env:PATH
cmake -S C:/work -B C:/work/$($BuildDir -replace '\\','/') -G Ninja -DCMAKE_BUILD_TYPE=$Config
if (`$LASTEXITCODE -ne 0) { exit `$LASTEXITCODE }
cmake --build C:/work/$($BuildDir -replace '\\','/')
if (`$LASTEXITCODE -ne 0) { exit `$LASTEXITCODE }
& `$env:QT_ROOT/bin/windeployqt.exe C:/work/$($BuildDir -replace '\\','/')/appccvWriter.exe --no-translations
if (`$LASTEXITCODE -ne 0) { exit `$LASTEXITCODE }
Write-Host 'Docker build OK'
"@

Write-Host "Project : $ProjectRoot"
Write-Host "Qt mount: ${HostQtPathDocker} -> C:/Qt"
Write-Host "Output  : $BuildDir"
Write-Host "Image   : $ImageTag"

docker run --rm `
    -v "${HostQtPathDocker}:C:/Qt:ro" `
    -v "${ProjectRootDocker}:C:/work" `
    -e "QT_ROOT=C:/Qt/6.9.1/mingw_64" `
    -e "MINGW_ROOT=C:/Qt/Tools/mingw1310_64" `
    $ImageTag `
    powershell -NoProfile -Command $BuildCmd

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Run on host: .\scripts\run.ps1 -BuildDir $BuildDir"
