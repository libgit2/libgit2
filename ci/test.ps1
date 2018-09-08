Set-StrictMode -Version Latest

$ErrorActionPreference = "Stop"
$PSDefaultParameterValues['*:ErrorAction'] = 'Stop'

[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

if ($Env:SKIP_TESTS) { exit }

Write-Host "##############################################################################"
Write-Host "## Configuring test environment"
Write-Host "##############################################################################"

Write-Host ""
Write-Host "Starting HTTP proxy..."
Invoke-WebRequest -Method GET -Uri https://github.com/ethomson/poxyproxy/releases/download/v0.1.0/poxyproxy-0.1.0.jar -OutFile poxyproxy.jar
javaw -jar poxyproxy.jar -d --port 8080 --credentials foo:bar

Write-Host ""
Write-Host "##############################################################################"
Write-Host "## Running default tests"
Write-Host "##############################################################################"

ctest -V -R libgit2_clar
if ($LastExitCode -ne 0) { [Environment]::Exit($LastExitCode) }

Write-Host ""
Write-Host "##############################################################################"
Write-Host "## Running credential callback tests"
Write-Host "##############################################################################"

$env:GITTEST_REMOTE_URL="https://github.com/libgit2/non-existent"
$env:GITTEST_REMOTE_USER="libgit2test"
ctest -V -R libgit2_clar-cred_callback

Write-Host ""
Write-Host "##############################################################################"
Write-Host "## Running proxy credential tests"
Write-Host "##############################################################################"

$env:GITTEST_REMOTE_PROXY_URL = "http://foo:bar@localhost:8080"
ctest -V -R libgit2_clar-proxy_credentials_in_url

Write-Host ""
Write-Host "##############################################################################"
Write-Host "## Running proxy tests"
Write-Host "##############################################################################"

$env:GITTEST_REMOTE_PROXY_URL = "http://localhost:8080"
$env:GITTEST_REMOTE_PROXY_USER = "foo"
$env:GITTEST_REMOTE_PROXY_PASS = "bar"
ctest -V -R libgit2_clar-proxy_credentials_request
if ($LastExitCode -ne 0) { [Environment]::Exit($LastExitCode) }

taskkill /F /IM javaw.exe
