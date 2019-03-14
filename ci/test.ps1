Set-StrictMode -Version Latest

$ErrorActionPreference = "Stop"
$PSDefaultParameterValues['*:ErrorAction'] = 'Stop'

[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$SourceDir = Split-Path (Split-Path (Get-Variable MyInvocation).Value.MyCommand.Path)
$BuildDir = Get-Location
$global:Success = $true

if ($Env:SKIP_TESTS) { exit }

# Ask ctest what it would run if we were to invoke it directly.  This lets
# us manage the test configuration in a single place (tests/CMakeLists.txt)
# instead of running clar here as well.  But it allows us to wrap our test
# harness with a leak checker like valgrind.  Append the option to write
# JUnit-style XML files.
function run_test {
	$TestName = $args[0]

	$TestCommand = (ctest -N -V -R "^$TestName$") -join "`n"

	if (-Not ($TestCommand -match "(?ms).*\n^[0-9]*: Test command: ")) {
		echo "Could not find tests: $TestName"
		exit
	}

	$TestCommand = (ctest -N -V -R "^$TestName$") -join "`n" -replace "(?ms).*\n^[0-9]*: Test command: ","" -replace "\n.*",""
	$TestCommand += " -r${BuildDir}\results_${TestName}.xml"

	Invoke-Expression $TestCommand
	if ($LastExitCode -ne 0) { $global:Success = $false }
}

Write-Host "##############################################################################"
Write-Host "## Configuring test environment"
Write-Host "##############################################################################"

if (-not $Env:SKIP_PROXY_TESTS) {
	Invoke-WebRequest -Method GET -Uri https://github.com/ethomson/poxyproxy/releases/download/v0.7.0/poxyproxy-0.7.0.jar -OutFile poxyproxy.jar

	Write-Host ""
	Write-Host "Starting HTTP proxy (Basic)..."
	javaw -jar poxyproxy.jar --port 8080 --credentials foo:bar --auth-type basic --quiet

	Write-Host ""
	Write-Host "Starting HTTP proxy (NTLM)..."
	javaw -jar poxyproxy.jar --port 8090 --credentials foo:bar --auth-type ntlm --quiet
}

if (-not $Env:SKIP_OFFLINE_TESTS) {
	Write-Host ""
	Write-Host "##############################################################################"
	Write-Host "## Running (offline) tests"
	Write-Host "##############################################################################"

	run_test offline
}

if ($Env:RUN_INVASIVE_TESTS) {
	Write-Host ""
	Write-Host "##############################################################################"
	Write-Host "## Running (invasive) tests"
	Write-Host "##############################################################################"

	$Env:GITTEST_INVASIVE_FS_SIZE=1
	$Env:GITTEST_INVASIVE_MEMORY=1
	$Env:GITTEST_INVASIVE_SPEED=1
	run_test invasive
	$Env:GITTEST_INVASIVE_FS_SIZE=$null
	$Env:GITTEST_INVASIVE_MEMORY=$null
	$Env:GITTEST_INVASIVE_SPEED=$null
}

if (-not $Env:SKIP_ONLINE_TESTS) {
	Write-Host ""
	Write-Host "##############################################################################"
	Write-Host "## Running (online) tests"
	Write-Host "##############################################################################"

	run_test online
}

if (-not $Env:SKIP_PROXY_TESTS) {
	# Test HTTP Basic authentication
	Write-Host ""
	Write-Host "Running proxy tests (Basic authentication)"
	Write-Host ""

	$Env:GITTEST_REMOTE_PROXY_HOST="localhost:8080"
	$Env:GITTEST_REMOTE_PROXY_USER="foo"
	$Env:GITTEST_REMOTE_PROXY_PASS="bar"
	run_test proxy

	# Test NTLM authentication
	Write-Host ""
	Write-Host "Running proxy tests (NTLM authentication)"
	Write-Host ""

	$Env:GITTEST_REMOTE_PROXY_HOST="localhost:8090"
	$Env:GITTEST_REMOTE_PROXY_USER="foo"
	$Env:GITTEST_REMOTE_PROXY_PASS="bar"
	run_test proxy

	$Env:GITTEST_REMOTE_PROXY_HOST=$null
	$Env:GITTEST_REMOTE_PROXY_USER=$null
	$Env:GITTEST_REMOTE_PROXY_PASS=$null

	taskkill /F /IM javaw.exe
}

if (-Not $global:Success) { exit 1 }
