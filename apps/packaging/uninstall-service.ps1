# Project Ambrose by Imjustchico
# Stops and removes the Ambrose Windows service without deleting its configuration or runtime data.

$ErrorActionPreference = "Stop"
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "uninstall-service.ps1 must run from an elevated PowerShell"
}

$binary = "$PSScriptRoot\..\..\build\windows-msvc-x64\bin\RelWithDebInfo\supervisor.exe"
if (-not (Test-Path -LiteralPath $binary)) {
    throw "Supervisor executable was not found: $binary"
}
& $binary --uninstall-service
if ($LASTEXITCODE -ne 0) { throw "supervisor --uninstall-service failed with exit code $LASTEXITCODE" }
Write-Output "Removed AmbroseSupervisor; configuration and runtime data were preserved."
