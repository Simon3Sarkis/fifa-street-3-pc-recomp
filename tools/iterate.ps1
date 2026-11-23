# Regenerate, rebuild and run FIFA Street 3 for a while, then print the end of the newest log.
# Usage: tools\iterate.ps1 [-RunSeconds 60] [-SkipCodegen] [-SkipBuild]
param([int]$RunSeconds = 60, [switch]$SkipCodegen, [switch]$SkipBuild)

$ErrorActionPreference = 'Continue'
$root = Split-Path $PSScriptRoot -Parent
$bin = Join-Path $root 'out\build\win-amd64-relwithdebinfo'
$sdk = Join-Path $root 'third_party\rexglue-sdk'
$rexglue = Join-Path $sdk 'out\win-amd64\rexgluerd.exe'
$devenv = Join-Path $root 'devenv.bat'
$work = Join-Path $root 'work'
New-Item -ItemType Directory -Force $work | Out-Null

Set-Location $root
if (-not $SkipCodegen) {
    & $rexglue --force codegen fifastreet3_manifest.toml *> (Join-Path $work 'codegen.log')
    Select-String -Path (Join-Path $work 'codegen.log') -Pattern 'cannot seal|Unresolved|ERRORS' | ForEach-Object { $_.Line }
}

if (-not $SkipBuild) {
    cmd /c "`"$devenv`" cmake --build out/build/win-amd64-relwithdebinfo" *> (Join-Path $work 'build.log')
    if ($LASTEXITCODE -ne 0) {
        Select-String -Path (Join-Path $work 'build.log') -Pattern ' error:' | Select-Object -First 10 | ForEach-Object { $_.Line }
        exit 1
    }
}

Set-Location $bin
# Runtime DLLs build into the SDK tree and are only copied when the exe relinks.
Get-ChildItem (Join-Path $sdk 'out\win-amd64') -Filter *.dll | Copy-Item -Destination . -Force
Copy-Item (Join-Path $root 'config\fifastreet3.runtime.toml') .\fifastreet3.toml -Force
# The game data is reached through a junction, so the exe folder looks like a release folder.
if (-not (Test-Path .\game)) { cmd /c mklink /J game "$(Join-Path $root 'game')" | Out-Null }

$p = Start-Process .\fifastreet3.exe -PassThru
$deadline = (Get-Date).AddSeconds($RunSeconds)
while (-not $p.HasExited -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 500 }
$alive = -not $p.HasExited
if ($alive) { Stop-Process -Id $p.Id -Force } else { "exit code: $($p.ExitCode)" }
"alive_after_${RunSeconds}s=$alive"

$log = Get-ChildItem logs -Filter *.log -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
if ($log) {
    "log: $($log.FullName)"
    Select-String -Path $log.FullName -Pattern 'critical|FATAL|error' | Select-Object -Last 8 | ForEach-Object { $_.Line }
    Get-Content $log.FullName -Tail 8
}
