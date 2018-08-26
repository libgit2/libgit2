Set-StrictMode -Version Latest

$ErrorActionPreference = "Stop"
$PSDefaultParameterValues['*:ErrorAction'] = 'Stop'

if ($Env:SOURCE_DIR) { $SourceDirectory = $Env:SOURCE_DIR } else { $SourceDirectory = Split-Path (Split-Path $MyInvocation.MyCommand.Path -Parent) -Parent }
$BuildDirectory = $(Get-Location).Path

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

$TestOutputDirectory = $BuildDirectory -replace "\\", "/"

Invoke-Expression "cmake ${SourceDirectory} -DBUILD_EXAMPLES=ON -DCLAR_XML=${TestOutputDirectory} ${Env:CMAKE_OPTIONS}"
if ($LastExitCode -ne 0) { [Environment]::Exit($LastExitCode) }

Write-Host ""
Write-Host "##############################################################################"
Write-Host "## Building libgit2"
Write-Host "##############################################################################"

cmake --build .
if ($LastExitCode -ne 0) { [Environment]::Exit($LastExitCode) }
