Set-StrictMode -Version Latest

$ErrorActionPreference = "Stop"
$PSDefaultParameterValues['*:ErrorAction'] = 'Stop'

if ($Env:SOURCE_DIR) { $SourceDirectory = $Env:SOURCE_DIR } else { $SourceDirectory = Split-Path (Split-Path $MyInvocation.MyCommand.Path -Parent) -Parent }
$BuildDirectory = $(Get-Location).Path

# Resolve CMake before modifying PATH
$CMAKE = (Get-Command cmake).Path

# Prepend BUILD_PATH to make available our own tools
if ($Env:BUILD_PATH) { $Env:PATH = $Env:BUILD_PATH }

Write-Host "Source directory: ${SourceDirectory}"
Write-Host "Build directory:  ${BuildDirectory}"
Write-Host ""
Write-Host "Operating system version:"
Get-CimInstance Win32_OperatingSystem | Select-Object Caption, Version, ServicePackMajorVersion, BuildNumber, OSArchitecture | Format-List
Write-Host "PATH: ${Env:PATH}"
Write-Host ""

Write-Host "##############################################################################"
Write-Host "## Configuring build environment"
Write-Host "##############################################################################"

Invoke-Expression "& '${CMAKE}' ${SourceDirectory} -DBUILD_EXAMPLES=ON ${Env:CMAKE_OPTIONS}"
if ($LastExitCode -ne 0) { [Environment]::Exit($LastExitCode) }

Write-Host ""
Write-Host "##############################################################################"
Write-Host "## Building libgit2"
Write-Host "##############################################################################"

Invoke-Expression "& '${CMAKE}' --build ."
if ($LastExitCode -ne 0) { [Environment]::Exit($LastExitCode) }
