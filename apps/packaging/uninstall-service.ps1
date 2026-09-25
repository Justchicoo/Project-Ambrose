# Project Ambrose by Imjustchico
# Stops and removes the Ambrose Windows service without deleting its configuration or runtime data.

[CmdletBinding()]
param([string]$ServiceName = "AmbroseSupervisor")

$ErrorActionPreference = "Stop"
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "uninstall-service.ps1 must run from an elevated PowerShell"
}

$service = Get-Service -Name $ServiceName -ErrorAction SilentlyContinue
if ($null -ne $service -and $service.Status -ne "Stopped") {
    Stop-Service -Name $ServiceName -Force
}
sc.exe delete $ServiceName | Out-Host
Write-Output "Removed $ServiceName; configuration and runtime data were preserved."
