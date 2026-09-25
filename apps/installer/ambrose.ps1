# Project Ambrose by Imjustchico
# Installs, configures and runs an Ambrose checkout on Windows.
$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$EnvFile = if ($env:AMBROSE_INSTALL_ENV) { $env:AMBROSE_INSTALL_ENV } else { Join-Path $Root 'conf\dist\env.dist' }
$InstallPrefixOverride = $env:AMBROSE_INSTALL_PREFIX
$BuildTypeOverride = $env:AMBROSE_BUILD_TYPE
$PresetOverride = $env:AMBROSE_PRESET
if (Test-Path $EnvFile) {
    Get-Content $EnvFile | Where-Object { $_ -match '^\s*AMBROSE_[A-Z0-9_]+=.*$' } | ForEach-Object {
        $name, $value = $_ -split '=', 2
        [Environment]::SetEnvironmentVariable($name, $value)
    }
}
$Prefix = if ($InstallPrefixOverride) { $InstallPrefixOverride } elseif ($env:AMBROSE_INSTALL_PREFIX) { $env:AMBROSE_INSTALL_PREFIX } else { Join-Path $Root 'env\dist' }
if (-not [System.IO.Path]::IsPathRooted($Prefix)) { $Prefix = Join-Path $Root $Prefix }
$BuildType =if ($BuildTypeOverride) { $BuildTypeOverride } elseif ($env:AMBROSE_BUILD_TYPE) { $env:AMBROSE_BUILD_TYPE } else { 'Debug' }
$Preset = if ($PresetOverride) { $PresetOverride } elseif ($env:AMBROSE_PRESET) { $env:AMBROSE_PRESET } else { 'windows-msvc-x64' }
$BuildDir = Join-Path $Root "build\$Preset"
$BinDir = Join-Path $Prefix 'bin'
$EtcDir = Join-Path $Prefix 'etc'

function Fail([string]$Message) {
    throw "ambrose installer: $Message"
}

function Invoke-Checked([string]$Command, [string[]]$Arguments) {
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) { Fail "$Command failed with exit code $LASTEXITCODE" }
}

function Invoke-Deps {
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { Fail "missing 'cmake'; install it and run deps again" }
    if (-not $env:VCPKG_ROOT) { Fail 'VCPKG_ROOT is not set; install vcpkg and set VCPKG_ROOT' }
    if (-not (Test-Path (Join-Path $env:VCPKG_ROOT 'scripts\buildsystems\vcpkg.cmake'))) { Fail 'VCPKG_ROOT does not contain vcpkg' }
    Write-Output 'dependencies found: CMake, Visual Studio and vcpkg; every library, such as OpenSSL, Botan and MariaDB, is supplied by vcpkg'
}

function Invoke-Compile {
    Invoke-Deps
    Invoke-Checked cmake @('--preset', $Preset, "-DCMAKE_INSTALL_PREFIX=$Prefix")
    $BuildPreset = if ($BuildType -eq 'Debug') { 'windows-debug' } else { 'windows-release' }
    Invoke-Checked cmake @('--build', '--preset', $BuildPreset)
    Invoke-Checked cmake @('--install', $BuildDir, '--config', $BuildType, '--prefix', $Prefix)
}

function Invoke-Conf {
    New-Item -ItemType Directory -Force $BinDir | Out-Null
    if (-not (Test-Path $EtcDir)) { Fail 'no installed configuration templates; run compile first' }
    Get-ChildItem $EtcDir -Filter '*.conf.dist' | ForEach-Object {
        $target = Join-Path $BinDir $_.BaseName
        if (-not (Test-Path $target)) { Copy-Item $_.FullName $target }
    }
}

function Invoke-Db {
    Invoke-Conf
    Invoke-Checked (Join-Path $BinDir 'dbimport.exe') @('--config', (Join-Path $BinDir 'dbimport.conf'))
}

function Invoke-Run([string]$App) {
    if ($App -notin @('loginserver', 'gameserver', 'patchserver', 'supervisor')) { Fail 'run expects loginserver, gameserver, patchserver or supervisor' }
    Invoke-Conf
    Invoke-Checked (Join-Path $BinDir "$App.exe") @('--config', (Join-Path $BinDir "${App}.conf"))
}

switch ($args[0]) {
    'deps' { Invoke-Deps }
    'compile' { Invoke-Compile }
    'conf' { Invoke-Conf }
    'db' { Invoke-Db }
    'run' { if ($args.Count -ne 2) { Fail 'run expects one app' }; Invoke-Run $args[1] }
    default { Fail 'usage: ambrose.ps1 {deps|compile|conf|db|run <app>}' }
}
