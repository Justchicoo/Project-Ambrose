# Project Ambrose by Imjustchico
# Launches the maintainer's own Wizard101 client against a local Ambrose login server with patching disabled, reading the install path from conf/launcher.conf.
[CmdletBinding()]
param(
    [string]$ConfigFile = (Join-Path $PSScriptRoot '..\..\conf\launcher.conf'),
    [string]$LoginHost,
    [int]$LoginPort = 0,
    [string]$Locale,
    [switch]$WhatIf
)

Set-StrictMode -Version 3.0
$ErrorActionPreference = 'Stop'

function Read-LauncherConfig {
    param([string]$Path)
    $values = @{}
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Launcher config not found: $Path. Copy conf/dist/launcher.conf.dist to it and set ClientDir."
    }
    foreach ($line in Get-Content -LiteralPath $Path) {
        $trimmed = $line.Trim()
        if ($trimmed -eq '' -or $trimmed.StartsWith('#')) {
            continue
        }
        $equals = $trimmed.IndexOf('=')
        if ($equals -lt 1) {
            throw "Bad line in ${Path}: $line"
        }
        $values[$trimmed.Substring(0, $equals).Trim()] = $trimmed.Substring($equals + 1).Trim().Trim('"')
    }
    return $values
}

$config = Read-LauncherConfig -Path $ConfigFile
if (-not $config.ContainsKey('ClientDir') -or [string]::IsNullOrWhiteSpace($config['ClientDir'])) {
    throw "ClientDir is not set in $ConfigFile"
}
$clientDir = $config['ClientDir']
$binDir = Join-Path $clientDir 'Bin'
$client = Join-Path $binDir 'WizardGraphicalClient.exe'
if (-not (Test-Path -LiteralPath $client)) {
    throw "WizardGraphicalClient.exe not found under $binDir. ClientDir must be the install folder that contains Bin."
}

if (-not $LoginHost) { $LoginHost = if ($config.ContainsKey('LoginHost')) { $config['LoginHost'] } else { '127.0.0.1' } }
if ($LoginPort -eq 0) { $LoginPort = if ($config.ContainsKey('LoginPort')) { [int]$config['LoginPort'] } else { 12000 } }
if (-not $Locale -and $config.ContainsKey('Locale')) { $Locale = $config['Locale'] }

$arguments = @('-L', $LoginHost, "$LoginPort", '-P', '0')
if ($Locale) {
    $arguments += @('-A', $Locale)
}

Write-Host "Launching $client $($arguments -join ' ')"
if ($WhatIf) {
    return
}
Start-Process -FilePath $client -ArgumentList $arguments -WorkingDirectory $binDir
