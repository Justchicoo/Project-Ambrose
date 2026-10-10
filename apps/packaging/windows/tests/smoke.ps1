# Project Ambrose by Imjustchico
# Installs a built setup program silently on a Windows machine that has never built Ambrose, checks every installed file against the manifest, the Installed apps entry and the Start menu shortcuts, starts the servers with their private MariaDB and passes only when the supervisor reports loginserver, gameserver and patchserver running, then stops them, uninstalls keeping the database, and uninstalls again with /REMOVEDATA, printing the logs of whatever failed.
param(
    [Parameter(Mandatory = $true)][string]$Setup,
    [string]$InstallDir = (Join-Path $env:RUNNER_TEMP 'Project Ambrose'),
    [int]$ReadyTimeout = 900
)
$ErrorActionPreference = 'Stop'
$failures = [System.Collections.Generic.List[string]]::new()
$database = Join-Path $env:LOCALAPPDATA 'ProjectAmbrose\database'
$uninstallKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\ProjectAmbrose'

function Step([string]$Text) { Write-Output "== $Text" }
function Check([bool]$Passed, [string]$What) {
    if ($Passed) { Write-Output "ok: $What" } else { Write-Output "FAILED: $What"; $failures.Add($What) }
}
function Script([string[]]$Arguments) {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $InstallDir 'ambrose.ps1') @Arguments
    return $LASTEXITCODE
}
function Show-Logs {
    foreach ($folder in @((Join-Path $InstallDir 'bin\logs'), $database)) {
        Get-ChildItem $folder -Recurse -File -Include '*.log', '*.err' -ErrorAction SilentlyContinue | ForEach-Object {
            Write-Output "--- the end of $($_.FullName)"
            Get-Content $_.FullName -Tail 60
        }
    }
}

Step "installing $Setup silently into $InstallDir"
$process = Start-Process -FilePath $Setup -ArgumentList '/S', "/D=$InstallDir" -Wait -PassThru
Check ($process.ExitCode -eq 0) "the setup exited 0 (it exited $($process.ExitCode))"
foreach ($file in 'bin\loginserver.exe', 'bin\gameserver.exe', 'bin\patchserver.exe', 'bin\supervisor.exe', 'bin\dbimport.exe', 'bin\client.exe',
    'mariadb\bin\mariadbd.exe', 'ambrose.ps1', 'ambrose-manifest.json', 'uninstall.exe') {
    Check (Test-Path (Join-Path $InstallDir $file)) "the install holds $file"
}
$manifest = Get-Content (Join-Path $InstallDir 'ambrose-manifest.json') -Raw | ConvertFrom-Json
$changed = @($manifest.files | Where-Object {
    $path = Join-Path $InstallDir ($_.path -replace '/', '\')
    -not (Test-Path $path) -or (Get-FileHash $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $_.sha256
})
Check ($changed.Count -eq 0) "every one of the $($manifest.files.Count) files the manifest lists is installed with its SHA-256$(if ($changed) { '; not: ' + (($changed | Select-Object -First 5 | ForEach-Object path) -join ', ') })"
$entry = Get-ItemProperty $uninstallKey -ErrorAction SilentlyContinue
Check ($entry -and $entry.DisplayName -eq 'Project Ambrose' -and $entry.DisplayVersion -eq $manifest.version) "Installed apps lists Project Ambrose $($manifest.version)"
Check (Test-Path (Join-Path ([Environment]::GetFolderPath('Programs')) 'Project Ambrose\Start Ambrose.lnk')) 'the Start menu has Start Ambrose'

Step 'starting Ambrose with its private MariaDB'
$code = Script @('start', '-Background')
Check ($code -eq 0) "start -Background exited 0 (it exited $code)"
Check (Test-Path (Join-Path $database 'my.ini')) "the private database was made in $database"
$code = Script @('status', '-WaitReady', "$ReadyTimeout")
Check ($code -eq 0) 'the supervisor reports loginserver, gameserver and patchserver running'
if ($code -ne 0) { Show-Logs }

Step 'stopping Ambrose'
$code = Script @('stop')
Check ($code -eq 0) "stop exited 0 (it exited $code)"
$left = @(Get-Process -Name 'supervisor', 'loginserver', 'gameserver', 'patchserver', 'mariadbd' -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path.StartsWith($InstallDir) })
Check ($left.Count -eq 0) "nothing from the install still runs$(if ($left) { ': ' + (($left | ForEach-Object Name) -join ', ') })"

Step 'uninstalling, keeping the database'
$process = Start-Process -FilePath (Join-Path $InstallDir 'uninstall.exe') -ArgumentList '/S', "_?=$InstallDir" -Wait -PassThru
Check ($process.ExitCode -eq 0) "the uninstaller exited 0 (it exited $($process.ExitCode))"
Check (-not (Test-Path (Join-Path $InstallDir 'bin\gameserver.exe'))) 'the programs are gone'
Check (-not (Test-Path (Join-Path $InstallDir 'mariadb'))) 'the MariaDB programs are gone'
Check (Test-Path (Join-Path $database 'my.ini')) 'the database is kept'
Check (Test-Path (Join-Path $InstallDir 'bin\conf.d\database.conf')) 'the settings are kept'
Check (-not (Test-Path $uninstallKey)) 'Installed apps no longer lists it'

Step 'uninstalling again with /REMOVEDATA'
$process = Start-Process -FilePath (Join-Path $InstallDir 'uninstall.exe') -ArgumentList '/S', '/REMOVEDATA', "_?=$InstallDir" -Wait -PassThru
Check ($process.ExitCode -eq 0) "the uninstaller exited 0 (it exited $($process.ExitCode))"
Check (-not (Test-Path $database)) 'the database is gone'
Check (-not (Test-Path (Join-Path $InstallDir 'bin'))) 'the settings are gone'

if ($failures.Count -gt 0) {
    Write-Output "the setup smoke test failed $($failures.Count) check(s)"
    exit 1
}
Write-Output 'the setup installed, ran loginserver, gameserver and patchserver, stopped and uninstalled cleanly'
