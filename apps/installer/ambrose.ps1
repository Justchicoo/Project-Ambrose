# Project Ambrose by Imjustchico
# Installs, configures and runs an Ambrose checkout on Windows: deps checks the tools and, with -Install, installs what is missing through winget (Visual Studio 2022 Build Tools with the C++ workload, CMake, Git) and vcpkg, and with -WithDatabase MariaDB, registered as a service so a server is running, and the account the shipped configuration names, made as root with no password when it installed MariaDB itself, asking for the root password when MariaDB was already there or the passwordless login is refused; -Plan prints those steps without taking them. It reads PATH, and VCPKG_ROOT when the shell has none, afresh before looking, so a tool an earlier run installed is found from a shell opened before it, and winget's answer that a package is already installed counts as done; when winget gives up on Build Tools while the Visual Studio installer it started is still working, it waits for that installer and trusts vswhere's answer instead. Beside the setup program's files, marked by their manifest, it runs the installed copy instead: start makes and starts a private MariaDB in the Ambrose data folder, on its own port and open only to this machine, gives it the account, points every app at it through conf.d, imports the databases and runs the supervisor, in the foreground or with -Background; stop asks the supervisor to shut down and stops that MariaDB; status prints each app's state, waiting with -WaitReady until all run; client lists the Wizard101 installs found, or with a folder that holds one writes it to conf.d for every app.
$ErrorActionPreference = 'Stop'
$Packaged = Test-Path (Join-Path $PSScriptRoot 'ambrose-manifest.json')
$Root = if ($Packaged) { $PSScriptRoot } else { (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path }
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
$Prefix = if ($InstallPrefixOverride) { $InstallPrefixOverride } elseif ($Packaged) { $Root } elseif ($env:AMBROSE_INSTALL_PREFIX) { $env:AMBROSE_INSTALL_PREFIX } else { Join-Path $Root 'env\dist' }
if (-not [System.IO.Path]::IsPathRooted($Prefix)) { $Prefix = Join-Path $Root $Prefix }
$BuildType =if ($BuildTypeOverride) { $BuildTypeOverride } elseif ($env:AMBROSE_BUILD_TYPE) { $env:AMBROSE_BUILD_TYPE } else { 'Debug' }
$Preset = if ($PresetOverride) { $PresetOverride } elseif ($env:AMBROSE_PRESET) { $env:AMBROSE_PRESET } else { 'windows-msvc-x64' }
$BuildDir = Join-Path $Root "build\$Preset"
$BinDir = Join-Path $Prefix 'bin'
$EtcDir = Join-Path $Prefix 'etc'
$DataRoot = if ($env:LOCALAPPDATA) { Join-Path $env:LOCALAPPDATA 'ProjectAmbrose' } else { Join-Path $HOME '.local/share/project-ambrose' }
$DatabaseDir = Join-Path $DataRoot 'database'
$DatabasePort = if ($env:AMBROSE_DATABASE_PORT) { [int]$env:AMBROSE_DATABASE_PORT } else { 3316 }
$MariaDbBin = Join-Path $Root 'mariadb\bin'

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
    $conf = if ($Packaged) { Join-Path $EtcDir 'dbimport.conf.dist' } else { Join-Path $Root 'src\tools\dbimport\dbimport.conf.dist' }
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

function Install-Winget([string]$Id, [string[]]$Extra = @(), [scriptblock]$Installed = $null) {
    & winget @('install', '--exact', '--id', $Id, '--accept-package-agreements', '--accept-source-agreements', '--silent') @Extra
    $code = $LASTEXITCODE
    if ($code -ne 0 -and $code -ne $WingetNothingToUpgrade) {
        if (-not $Installed) { Fail "winget failed with exit code $code" }
        Write-Output "winget ended with exit code $code; waiting for the installer it started to finish before deciding"
        Wait-VsSetup
        if (-not (& $Installed)) { Fail "winget failed with exit code $code and $Id is not installed; run deps -Install again" }
        Write-Output "$Id finished installing after winget ended"
    }
    Update-Path
}

function Wait-VsSetup {
    $names = 'vs_BuildTools', 'vs_setup_bootstrapper', 'vs_installer', 'vs_installershell', 'winsdksetup', 'setup'
    $installer = if (${env:ProgramFiles(x86)}) { Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer' } else { $null }
    $deadline = (Get-Date).AddHours(2)
    while ((Get-Date) -lt $deadline) {
        $running = Get-Process -Name $names -ErrorAction SilentlyContinue | Where-Object { $_.Name -ne 'setup' -or ($installer -and $_.Path -and $_.Path.StartsWith($installer, [StringComparison]::OrdinalIgnoreCase)) }
        if (-not $running) { return }
        Start-Sleep -Seconds 15
    }
    Fail 'the Visual Studio installer was still running after two hours'
}

function Test-VsTools {
    return [bool]($VsWhere -and (Test-Path $VsWhere) -and (& $VsWhere -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath))
}

function Install-Tools {
    Update-Path
    $vs = Test-Found 'vs' { Test-VsTools }
    $cmake = Test-Found 'cmake' { Get-Command cmake -ErrorAction SilentlyContinue }
    $git = Test-Found 'git' { Get-Command git -ErrorAction SilentlyContinue }
    $cloned = Test-Found 'vcpkg' { Test-Path (Join-Path $VcpkgDir 'scripts\buildsystems\vcpkg.cmake') }
    $built = Test-Found 'vcpkg' { Test-Path (Join-Path $VcpkgDir 'vcpkg.exe') }
    if (-not ($vs -and $cmake -and $git) -and -not (Get-Command winget -ErrorAction SilentlyContinue)) {
        Fail 'winget is missing; install App Installer from the Microsoft Store, or install Visual Studio 2022 Build Tools, CMake and Git yourself, then run deps again'
    }
    Invoke-Step $vs 'Visual Studio 2022 Build Tools with the C++ workload (winget Microsoft.VisualStudio.2022.BuildTools, --add Microsoft.VisualStudio.Workload.VCTools)' 'the C++ tools through vswhere' {
        Install-Winget 'Microsoft.VisualStudio.2022.BuildTools' @('--override', '--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --quiet --wait --norestart') { Test-VsTools }
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
    if ($mariadb) {
        Write-Output "create: the $user account for the ${user}_* databases, as root through the MariaDB client, which asks for the root password"
    } else {
        Write-Output "create: the $user account for the ${user}_* databases, as root through the MariaDB client with no password, since the MariaDB just installed leaves root without one and open only on this machine; give root a password afterwards with ALTER USER"
    }
    if ($Plan) { return }
    $client = Find-MariaDbClient
    if (-not $client) { Fail 'no mariadb or mysql client found; add its bin folder to PATH and run deps -WithDatabase again' }
    $sql = Get-AccountSql $account
    if (-not $mariadb) {
        & $client @('-u', 'root', '-e', $sql)
        if ($LASTEXITCODE -eq 0) { Write-Output "MariaDB has the $user account the shipped configuration names"; return }
        Write-Output 'root on this MariaDB has a password after all, so the MariaDB client asks for it'
    }
    Invoke-Checked $client @('-u', 'root', '-p', '-e', $sql)
    Write-Output "MariaDB has the $user account the shipped configuration names"
}

function Get-AccountSql($Account) {
    $user = $Account.User
    $password = $Account.Password
    $grant = "``${user}\_%``.*"
    return "CREATE USER IF NOT EXISTS '$user'@'localhost' IDENTIFIED BY '$password'; CREATE USER IF NOT EXISTS '$user'@'127.0.0.1' IDENTIFIED BY '$password'; GRANT ALL PRIVILEGES ON $grant TO '$user'@'localhost'; GRANT ALL PRIVILEGES ON $grant TO '$user'@'127.0.0.1';"
}

function Get-MariaDbTool([string]$Name) {
    foreach ($file in @("$Name.exe", "$Name.cmd", $Name)) {
        $path = Join-Path $MariaDbBin $file
        if (Test-Path $path -PathType Leaf) { return $path }
    }
    Fail "the setup's MariaDB has no $Name in $MariaDbBin; repair Project Ambrose from Installed apps or run its setup again"
}

function Test-Database {
    & (Get-MariaDbTool 'mariadb-admin') @('--host=127.0.0.1', "--port=$DatabasePort", '--user=root', '--connect-timeout=2', 'ping') *> $null
    return $LASTEXITCODE -eq 0
}

function Start-Database {
    $ini = Join-Path $DatabaseDir 'my.ini'
    if (-not (Test-Path $ini)) {
        if ((Test-Path $DatabaseDir) -and (Get-ChildItem $DatabaseDir -Force | Select-Object -First 1)) {
            Fail "$DatabaseDir holds files but no my.ini, so it is not a database this setup made; move it away and start again"
        }
        Write-Output "create: the private MariaDB database in $DatabaseDir on port $DatabasePort, open only to this machine"
        Invoke-Checked (Get-MariaDbTool 'mariadb-install-db') @("--datadir=$DatabaseDir", "--port=$DatabasePort")
        $folder = $DatabaseDir -replace '\\', '/'
        Add-Content -Path $ini -Value "`n[mysqld]`ndatadir=$folder`nport=$DatabasePort`nbind-address=127.0.0.1`n"
    }
    if (Test-Database) { Write-Output "the private MariaDB is running on port $DatabasePort"; return }
    Write-Output "start: the private MariaDB on port $DatabasePort"
    $options = @{ FilePath = (Get-MariaDbTool 'mariadbd'); ArgumentList = @("--defaults-file=`"$ini`"") }
    if ($env:OS -eq 'Windows_NT') { $options.WindowStyle = 'Hidden' }
    Start-Process @options | Out-Null
    $deadline = (Get-Date).AddSeconds(90)
    while (-not (Test-Database)) {
        if ((Get-Date) -gt $deadline) { Fail "the private MariaDB did not answer on port $DatabasePort within 90 seconds; its log is the .err file in $DatabaseDir" }
        Start-Sleep -Seconds 1
    }
    Write-Output "the private MariaDB is running on port $DatabasePort"
}

function Stop-Database {
    if (-not (Test-Database)) { return }
    & (Get-MariaDbTool 'mariadb-admin') @('--host=127.0.0.1', "--port=$DatabasePort", '--user=root', 'shutdown')
    if ($LASTEXITCODE -eq 0) { Write-Output 'stopped: the private MariaDB' }
}

function Write-DatabaseConf {
    $account = Get-DatabaseAccount
    & (Get-MariaDbTool 'mariadb') @('--host=127.0.0.1', "--port=$DatabasePort", '--user=root', '-e', (Get-AccountSql $account))
    if ($LASTEXITCODE -ne 0) { Fail "the private MariaDB refused to make the $($account.User) account" }
    $conf = Join-Path $EtcDir 'dbimport.conf.dist'
    $lines = Get-Content $conf | Where-Object { $_ -match '^\s*[A-Za-z]+DatabaseInfo\s*=' } | ForEach-Object {
        $name, $value = $_ -split '=', 2
        $fields = $value.Trim().Trim('"') -split ';'
        $fields[1] = "$DatabasePort"
        "$($name.Trim()) = `"$($fields -join ';')`""
    }
    $folder = Join-Path $BinDir 'conf.d'
    New-Item -ItemType Directory -Force $folder | Out-Null
    $header = @('# Project Ambrose by Imjustchico', '# The private MariaDB the setup installed, rewritten by ambrose.ps1 start on every start so every app and dbimport reach it on its own port.')
    [System.IO.File]::WriteAllText((Join-Path $folder 'database.conf'), ((@($header) + @($lines)) -join "`n") + "`n")
}

function Get-AdminPort {
    $conf = Join-Path $BinDir 'supervisor.conf'
    $line = if (Test-Path $conf) { Get-Content $conf | Where-Object { $_ -match '^\s*Admin\.Port\s*=\s*\d+\s*$' } | Select-Object -Last 1 } else { $null }
    if ($line) { return [int](($line -split '=', 2)[1].Trim()) }
    return 12020
}

function Invoke-Supervisor([string]$Method, [string]$Path) {
    $file = Join-Path $DataRoot 'admin\supervisor.token'
    if (-not (Test-Path $file)) { return $null }
    $token = (Get-Content $file -Raw).Trim()
    try { return Invoke-RestMethod -Method $Method -Uri "http://127.0.0.1:$(Get-AdminPort)$Path" -Headers @{ Authorization = "Bearer $token" } -TimeoutSec 10 }
    catch { return $null }
}

function Get-Supervisors {
    $program = Join-Path $BinDir 'supervisor'
    return @(Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -and ($_.Path -eq "$program.exe" -or $_.Path -eq $program) })
}

function Invoke-Start([string[]]$Options = @()) {
    $background = $false
    foreach ($option in $Options) {
        if ($option -in @('-Background', '--background')) { $background = $true }
        else { Fail 'start takes -Background' }
    }
    if (-not $Packaged) { Fail 'start runs a setup-installed Ambrose with its own MariaDB; in a checkout use deps -WithDatabase, db and run supervisor' }
    if (Get-Supervisors) { Write-Output 'Ambrose is already running; stop it first or open its panel'; return }
    Start-Database
    Invoke-Conf
    Write-DatabaseConf
    Invoke-Db
    if ($background) {
        $options = @{ FilePath = (Join-Path $BinDir 'supervisor.exe'); ArgumentList = @('--config', "`"$(Join-Path $BinDir 'supervisor.conf')`""); WorkingDirectory = $BinDir }
        if ($env:OS -eq 'Windows_NT') { $options.WindowStyle = 'Minimized' }
        Start-Process @options | Out-Null
        Write-Output 'started: the supervisor, which starts loginserver, gameserver and patchserver'
        return
    }
    try { Invoke-Run 'supervisor' }
    finally { Stop-Database }
}

function Invoke-Stop {
    $running = Get-Supervisors
    if ($running) {
        if ($null -eq (Invoke-Supervisor 'Post' '/api/shutdown')) { Write-Output 'the supervisor did not take the shutdown request; ending it' ; $running | Stop-Process -ErrorAction SilentlyContinue }
        $running | Wait-Process -Timeout 180 -ErrorAction SilentlyContinue
        Write-Output 'stopped: the supervisor and the servers it runs'
    }
    if ($Packaged) { Stop-Database }
}

function Invoke-Status([string[]]$Options = @()) {
    $wait = 0
    for ($index = 0; $index -lt $Options.Count; $index++) {
        if ($Options[$index] -in @('-WaitReady', '--wait-ready') -and $index + 1 -lt $Options.Count) { $wait = [int]$Options[++$index] }
        else { Fail 'status takes -WaitReady <seconds>' }
    }
    $deadline = (Get-Date).AddSeconds($wait)
    while ($true) {
        $apps = @(Invoke-Supervisor 'Get' '/api/apps' | ForEach-Object { $_ } | Where-Object { $_.supervision })
        $ready = $apps.Count -gt 0 -and -not ($apps | Where-Object { $_.supervision.state -ne 'running' })
        if ($ready -or (Get-Date) -ge $deadline) { break }
        Start-Sleep -Seconds 5
    }
    if ($apps.Count -eq 0) { Write-Output 'the supervisor is not answering'; exit 1 }
    foreach ($app in $apps) { Write-Output "$($app.name): $($app.supervision.state)" }
    if (-not $ready) { exit 1 }
}

function Invoke-Client([string[]]$Options = @()) {
    if ($Options.Count -gt 1) { Fail 'client takes at most the folder of your Wizard101 install' }
    if ($Options.Count -eq 0) {
        & (Join-Path $BinDir 'client.exe') installs
        exit $LASTEXITCODE
    }
    $folder = $Options[0].Trim().Trim('"').TrimEnd('\', '/')
    if (-not $folder -or -not (Test-Path (Join-Path $folder 'Data\GameData') -PathType Container)) {
        Fail "$folder is not a Wizard101 install: it has no Data\GameData folder; choose the folder that holds Data and Bin"
    }
    $conf = Join-Path $BinDir 'conf.d'
    New-Item -ItemType Directory -Force $conf | Out-Null
    $lines = @('# Project Ambrose by Imjustchico', '# The Wizard101 install chosen in the setup program, which every app reads its client data from.', "ClientDir = $folder")
    [System.IO.File]::WriteAllText((Join-Path $conf 'client.conf'), ($lines -join "`n") + "`n")
    Write-Output "every app reads its client data from $folder"
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
    Update-Path
    if (-not $env:VCPKG_ROOT -and $env:OS -eq 'Windows_NT') { $env:VCPKG_ROOT = [Environment]::GetEnvironmentVariable('VCPKG_ROOT', 'User') }
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
    Push-Location $BinDir
    try { Invoke-Checked (Join-Path $BinDir "$App.exe") @('--config', (Join-Path $BinDir "${App}.conf")) }
    finally { Pop-Location }
}

switch ($args[0]) {
    'deps' { Invoke-Deps @($args | Select-Object -Skip 1) }
    'compile' { Invoke-Compile }
    'conf' { Invoke-Conf }
    'db' { Invoke-Db }
    'run' { if ($args.Count -ne 2) { Fail 'run expects one app' }; Invoke-Run $args[1] }
    'start' { Invoke-Start @($args | Select-Object -Skip 1) }
    'stop' { Invoke-Stop }
    'status' { Invoke-Status @($args | Select-Object -Skip 1) }
    'client' { Invoke-Client @($args | Select-Object -Skip 1) }
    default { Fail 'usage: ambrose.ps1 {deps [-Install] [-WithDatabase] [-Plan]|compile|conf|db|run <app>|start [-Background]|stop|status [-WaitReady <seconds>]|client [<folder>]}' }
}
