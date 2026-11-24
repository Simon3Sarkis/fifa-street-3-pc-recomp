# Run the game with a test input script, take screenshots, and wait for the script's quit.
# Usage: & .\tools\run_script.ps1 -Script tests\input\press_start.txt -At 10,20,30 [-Name run1] [-Timeout 180]
#        [-Save work\saves\main_menu]   start from a copy of this user-data folder (default: empty)
# Every run gets its own user-data folder, work\userdata\<Name> (saves, profiles), so runs repeat;
# the shader cache is shared (work\cache). Your normal Documents\fifastreet3 folder is not used.
# Screenshots go to work\snaps\<Name>_<t>s.png (t = seconds since the game started); the game's log
# is copied to work\runs\<Name>.log.
param([Parameter(Mandatory = $true)][string]$Script, [int[]]$At = @(), [string]$Name = '',
      [int]$Timeout = 300, [string]$Save = '', [string[]]$GameArgs = @())

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$bin = Join-Path $root 'out\build\win-amd64-relwithdebinfo'
$scriptPath = (Resolve-Path $Script).Path
if (-not $Name) { $Name = [IO.Path]::GetFileNameWithoutExtension($scriptPath) + '_' + (Get-Date -Format 'HHmmss') }

if (Get-Process fifastreet3 -ErrorAction SilentlyContinue) { throw 'fifastreet3 is already running' }
# Same staging as iterate.ps1: runtime DLLs, config, game junction.
Get-ChildItem (Join-Path $root 'third_party\rexglue-sdk\out\win-amd64') -Filter *.dll | Copy-Item -Destination $bin -Force
Copy-Item (Join-Path $root 'config\fifastreet3.runtime.toml') (Join-Path $bin 'fifastreet3.toml') -Force
if (-not (Test-Path (Join-Path $bin 'game'))) { cmd /c mklink /J "$(Join-Path $bin 'game')" "$(Join-Path $root 'game')" | Out-Null }

$userData = Join-Path $root "work\userdata\$Name"
if (Test-Path $userData) { throw "user data folder $userData exists; pick another -Name" }
New-Item -ItemType Directory -Force $userData | Out-Null
if ($Save) { Copy-Item (Join-Path (Resolve-Path $Save).Path '*') $userData -Recurse -Force; "save: $Save" }
$cache = Join-Path $root 'work\cache'; New-Item -ItemType Directory -Force $cache | Out-Null

$before = Get-ChildItem (Join-Path $bin 'logs') -Filter *.log -ErrorAction SilentlyContinue | ForEach-Object FullName
# Start-Process joins arguments with spaces, so a path with spaces needs its own quotes.
# `snap <name>` lines in the script save the game's own frames to work\snaps\<Name>\<name>.bmp
# (converted to .png at the end). They can't be covered by other windows, unlike -At screen copies.
$snapDir = Join-Path $root "work\snaps\$Name"
$args = @("--input_script=`"$scriptPath`"", "--user_data_root=`"$userData`"", "--cache_root=`"$cache`"",
          "--input_script_snap_dir=`"$snapDir`"") + $GameArgs
Start-Process (Join-Path $bin 'fifastreet3.exe') -WorkingDirectory $bin -ArgumentList $args
Start-Sleep -Seconds 1
"run $Name"
if ($At.Count -gt 0) { & (Join-Path $PSScriptRoot 'snap.ps1') -NoLaunch -At $At -Prefix $Name }

$deadline = (Get-Date).AddSeconds($Timeout)
while ((Get-Process fifastreet3 -ErrorAction SilentlyContinue) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 500 }
if (Get-Process fifastreet3 -ErrorAction SilentlyContinue) {
  Stop-Process -Name fifastreet3 -Force; "timed out after ${Timeout}s (script had no quit, or the game hung)"
}

$log = Get-ChildItem (Join-Path $bin 'logs') -Filter *.log | Where-Object { $before -notcontains $_.FullName } |
  Sort-Object LastWriteTime | Select-Object -Last 1
if (-not $log) { $log = Get-ChildItem (Join-Path $bin 'logs') -Filter *.log | Sort-Object LastWriteTime | Select-Object -Last 1 }
$runs = Join-Path $root 'work\runs'; New-Item -ItemType Directory -Force $runs | Out-Null
Copy-Item $log.FullName (Join-Path $runs "$Name.log") -Force
"log: work\runs\$Name.log"
if (Test-Path $snapDir) {
  python -c "import glob,os,sys;from PIL import Image;fs=glob.glob(os.path.join(sys.argv[1],'*.bmp'));[(Image.open(f).save(f[:-4]+'.png'),os.remove(f)) for f in fs];print(f'snaps: {len(fs)} in work/snaps/{os.path.basename(sys.argv[1])}')" $snapDir
}
Select-String -Path $log.FullName -Pattern 'input_script|critical|FATAL|\[error\]' | ForEach-Object { $_.Line.Substring([Math]::Min(26, $_.Line.Length)) }
