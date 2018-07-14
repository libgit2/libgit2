Set-StrictMode -Version Latest

$ErrorActionPreference = "Stop"
$PSDefaultParameterValues['*:ErrorAction'] = 'Stop'

[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

[Reflection.Assembly]::LoadWithPartialName("System.IO.Compression.FileSystem");

Write-Host "##############################################################################"
Write-Host "## Downloading mingw"
Write-Host "##############################################################################"

$mingw_uri = "https://bintray.com/libgit2/build-dependencies/download_file?file_path=mingw-w64-x86_64-8.1.0-release-win32-seh-rt_v6-rev0.zip"
$platform = "x86_64"

$wc = New-Object net.webclient
$wc.Downloadfile($mingw_uri, "${Env:TEMP}/mingw-${platform}.zip")

[System.IO.Compression.ZipFile]::ExtractToDirectory("${Env:TEMP}/mingw-${platform}.zip", $Env:TEMP)
