# Project Ambrose by Imjustchico
# Installs, configures and runs an Ambrose checkout on Windows: deps checks the tools and, with -Install, installs what is missing through winget (Visual Studio 2022 Build Tools with the C++ workload, CMake, Git) and vcpkg, and with -WithDatabase MariaDB, registered as a service so a server is running, and the account the shipped configuration names; -Plan prints those steps without taking them. It reads PATH afresh before looking, so a tool an earlier run installed is found, and winget's answer that a package is already installed counts as done.
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
$Plan = $false
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

$VsWhere = if (${env:ProgramFiles(x86)}) { Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe' } else { '' }
$VcpkgDir = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } elseif ($env:USERPROFILE) { Join-Path $env:USERPROFILE 'vcpkg' } else { Join-Path $HOME 'vcpkg' }

function Test-Found([string]$Name, [scriptblock]$Probe) {
    if ($env:AMBROSE_DEPS_FOUND) { return $Name -in ($env:AMBROSE_DEPS_FOUND -split ',' | ForEach-Object { $_.Trim() }) }
    return [bool](& $Probe)
}

function Find-MariaDbClient {
    $command = Get-Command mariadb, mysql -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($command) { return $command.Source }
    $installed = Get-ChildItem (Join-Path $env:ProgramFiles 'MariaDB*\bin\mariadb.exe'), (Join-Path $env:ProgramFiles 'MySQL\MySQL Server*\bin\mysql.exe') -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($installed) { return $installed.FullName }
    return $null
}

function Get-DatabaseAccount {
    $conf = Join-Path $Root 'src\tools\dbimport\dbimport.conf.dist'
    $line = Get-Content $conf | Where-Object { $_ -match '^\s*LoginDatabaseInfo\s*=\s*"[^"]*"' } | Select-Object -First 1
    if (-not $line) { Fail "no LoginDatabaseInfo in $conf" }
    $fields = ($line -replace '^[^"]*"', '' -replace '".*$', '') -split ';'
    if ($fields.Count -lt 5) { Fail "LoginDatabaseInfo in $conf is not host;port;user;password;database" }
    return @{ User = $fields[2]; Password = $fields[3] }
}

function Update-Path {
    if ($Plan -or $env:OS -ne 'Windows_NT') { return }
    $env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
}

function Invoke-Step([bool]$Found, [string]$What, [string]$Seen, [scriptblock]$Action) {
    if ($Found) { Write-Output "skip: $What, found $Seen"; return }
    Write-Output "install: $What"
    if (-not $Plan) { & $Action }
}

$WingetNothingToUpgrade = -1978335189

function Install-Winget([string]$Id, [string[]]$Extra = @()) {
    & winget @('install', '--exact', '--id', $Id, '--accept-package-agreements', '--accept-source-agreements', '--silent') @Extra
    if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne $WingetNothingToUpgrade) { Fail "winget failed with exit code $LASTEXITCODE" }
    Update-Path
}

function Install-Tools {
    Update-Path
    $vs = Test-Found 'vs' { $VsWhere -and (Test-Path $VsWhere) -and (& $VsWhere -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath) }
    $cmake = Test-Found 'cmake' { Get-Command cmake -ErrorAction SilentlyContinue }
    $git = Test-Found 'git' { Get-Command git -ErrorAction SilentlyContinue }
    $cloned = Test-Found 'vcpkg' { Test-Path (Join-Path $VcpkgDir 'scripts\buildsystems\vcpkg.cmake') }
    $built = Test-Found 'vcpkg' { Test-Path (Join-Path $VcpkgDir 'vcpkg.exe') }
    if (-not ($vs -and $cmake -and $git) -and -not (Get-Command winget -ErrorAction SilentlyContinue)) {
        Fail 'winget is missing; install App Installer from the Microsoft Store, or install Visual Studio 2022 Build Tools, CMake and Git yourself, then run deps again'
    }
    Invoke-Step $vs 'Visual Studio 2022 Build Tools with the C++ workload (winget Microsoft.VisualStudio.2022.BuildTools, --add Microsoft.VisualStudio.Workload.VCTools)' 'the C++ tools through vswhere' {
        Install-Winget 'Microsoft.VisualStudio.2022.BuildTools' @('--override', '--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --quiet --wait --norestart')
    }
    Invoke-Step $cmake 'CMake (winget Kitware.CMake)' 'cmake' { Install-Winget 'Kitware.CMake' }
    Invoke-Step $git 'Git (winget Git.Git)' 'git' { Install-Winget 'Git.Git' }
    Invoke-Step $cloned "vcpkg, cloned into $VcpkgDir" "vcpkg in $VcpkgDir" {
        Invoke-Checked git @('clone', '-q', 'https://github.com/microsoft/vcpkg.git', $VcpkgDir)
    }
    $Bootstrap = Join-Path $VcpkgDir 'bootstrap-vcpkg.bat'
    Invoke-Step $built "vcpkg, bootstrapped with $Bootstrap -disableMetrics" "vcpkg.exe in $VcpkgDir" {
        Invoke-Checked $Bootstrap @('-disableMetrics')
    }
    $env:VCPKG_ROOT = $VcpkgDir
    Write-Output "vcpkg is in $VcpkgDir; set VCPKG_ROOT before compile: [Environment]::SetEnvironmentVariable('VCPKG_ROOT', '$VcpkgDir', 'User')"
}

function Install-Database {
    Update-Path
    $account = Get-DatabaseAccount
    $user = $account.User
    $mariadb = Test-Found 'mariadb' { Get-Service -Name 'MariaDB*', 'MySQL*' -ErrorAction SilentlyContinue }
    if (-not $mariadb -and -not (Get-Command winget -ErrorAction SilentlyContinue)) {
        Fail 'winget is missing; install App Installer from the Microsoft Store, or install MariaDB yourself, then run deps again'
    }
    if (-not $mariadb -and -not $Plan -and (Find-MariaDbClient)) {
        Fail 'MariaDB or MySQL is installed but runs as no service, so no server answers; register it as a service from an administrator prompt (mariadb-install-db.exe --service=MariaDB --port=3306 with your data folder), or uninstall it, then run deps -WithDatabase again'
    }
    Invoke-Step $mariadb 'MariaDB as the service MariaDB on port 3306 (winget MariaDB.Server, SERVICENAME=MariaDB PORT=3306)' 'a MySQL or MariaDB service' {
        Install-Winget 'MariaDB.Server' @('--custom', 'SERVICENAME=MariaDB PORT=3306')
    }
    Write-Output "create: the $user account for the ${user}_* databases, as root through the MariaDB client, which asks for the root password"
    if ($Plan) { return }
    $client = Find-MariaDbClient
    if (-not $client) { Fail 'no mariadb or mysql client found; add its bin folder to PATH and run deps -WithDatabase again' }
    $password = $account.Password
    $grant = "``${user}\_%``.*"
    $sql = "CREATE USER IF NOT EXISTS '$user'@'localhost' IDENTIFIED BY '$password'; CREATE USER IF NOT EXISTS '$user'@'127.0.0.1' IDENTIFIED BY '$password'; GRANT ALL PRIVILEGES ON $grant TO '$user'@'localhost'; GRANT ALL PRIVILEGES ON $grant TO '$user'@'127.0.0.1';"
    Invoke-Checked $client @('-u', 'root', '-p', '-e', $sql)
    Write-Output "MariaDB has the $user account the shipped configuration names"
}

function Invoke-Deps([string[]]$Options = @()) {
    $install = $false
    $database = $false
    foreach ($option in $Options) {
        if ($option -in @('-Install', '--install')) { $install = $true }
        elseif ($option -in @('-WithDatabase', '--with-database')) { $database = $true }
        elseif ($option -in @('-Plan', '--plan')) { $script:Plan = $true }
        else { Fail 'deps takes -Install, -WithDatabase and -Plan' }
    }
    if ($install) { Install-Tools }
    if ($database) { Install-Database }
    if ($Plan) { Write-Output 'plan only; nothing was installed'; return }
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
    $InstallConfig = if ($BuildType -eq 'Debug') { 'Debug' } else { 'RelWithDebInfo' }
    Invoke-Checked cmake @('--install', $BuildDir, '--config', $InstallConfig, '--prefix', $Prefix)
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
    'deps' { Invoke-Deps @($args | Select-Object -Skip 1) }
    'compile' { Invoke-Compile }
    'conf' { Invoke-Conf }
    'db' { Invoke-Db }
    'run' { if ($args.Count -ne 2) { Fail 'run expects one app' }; Invoke-Run $args[1] }
    default { Fail 'usage: ambrose.ps1 {deps [-Install] [-WithDatabase] [-Plan]|compile|conf|db|run <app>}' }
}
