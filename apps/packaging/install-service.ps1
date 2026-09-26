# Project Ambrose by Imjustchico
# Registers the supervisor's native Windows service integration.

[CmdletBinding()]
param(
    [string]$Binary = "$PSScriptRoot\..\..\build\windows-msvc-x64\bin\RelWithDebInfo\supervisor.exe",
    [string]$Config = "$env:ProgramData\Ambrose\supervisor.conf"
)

$ErrorActionPreference = "Stop"
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "install-service.ps1 must run from an elevated PowerShell"
}
if (-not (Test-Path -LiteralPath $Binary)) {
    throw "Supervisor executable was not found: $Binary"
}

$configDirectory = Split-Path -Parent $Config
New-Item -ItemType Directory -Force -Path $configDirectory | Out-Null
& $Binary --install-service --config $Config
if ($LASTEXITCODE -ne 0) { throw "supervisor --install-service failed with exit code $LASTEXITCODE" }
Write-Output "Installed AmbroseSupervisor; start it with: Start-Service AmbroseSupervisor"
