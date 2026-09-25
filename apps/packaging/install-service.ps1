# Project Ambrose by Imjustchico
# Registers the foreground supervisor with Windows Service Control Manager under a dedicated local service account.

[CmdletBinding()]
param(
    [string]$Binary = "$PSScriptRoot\..\..\build\windows-msvc-x64\bin\Release\supervisor.exe",
    [string]$Config = "$env:ProgramData\Ambrose\supervisor.conf",
    [string]$ServiceName = "AmbroseSupervisor"
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
$distributed = Join-Path (Split-Path -Parent $PSScriptRoot) "..\..\conf\dist\supervisor.conf.dist"
if (-not (Test-Path -LiteralPath $Config)) {
    Copy-Item -LiteralPath $distributed -Destination $Config
}

sc.exe create $ServiceName binPath= "`"$Binary`" --config `"$Config`"" start= auto DisplayName= "Project Ambrose Supervisor" | Out-Host
sc.exe description $ServiceName "Runs the Project Ambrose supervisor and its headless server apps." | Out-Host
Write-Output "Installed $ServiceName; start it with: Start-Service $ServiceName"
