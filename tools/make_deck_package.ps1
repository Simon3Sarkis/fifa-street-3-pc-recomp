# Package the current Windows build as a portable folder for the Steam Deck (run under Proton).
# Usage: & .\tools\make_deck_package.ps1 [-Out dist\fifastreet3-deck]
# Contents: exe, runtime + GPU plugin DLLs, the VC++ runtime DLLs (app-local, so Proton's own versions
# don't matter), fifastreet3.toml (from config\fifastreet3.runtime.toml), portable.txt (saves in
# userdata\ next to the exe), tools\deck\*.sh and the test input scripts. No game files: copy your
# extracted game\ folder next to the exe on the Deck once (tools\deck\push.sh does that over SSH).
param([string]$Out = "$PSScriptRoot\..\dist\fifastreet3-deck")

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$bin = Join-Path $root 'out\build\win-amd64-relwithdebinfo'
$sdkBin = Join-Path $root 'third_party\rexglue-sdk\out\win-amd64'
$crt = Get-ChildItem 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Redist\MSVC\*\x64\Microsoft.VC143.CRT' -Directory |
  Sort-Object FullName | Select-Object -Last 1
if (-not $crt) { throw 'VC++ redist (Microsoft.VC143.CRT) not found' }

New-Item -ItemType Directory -Force $Out | Out-Null
$Out = (Resolve-Path $Out).Path
Copy-Item (Join-Path $bin 'fifastreet3.exe') $Out -Force
Get-ChildItem $sdkBin -Filter *.dll | Copy-Item -Destination $Out -Force
foreach ($d in 'msvcp140.dll', 'msvcp140_atomic_wait.dll', 'vcruntime140.dll', 'vcruntime140_1.dll') {
  Copy-Item (Join-Path $crt.FullName $d) $Out -Force
}
Copy-Item (Join-Path $root 'config\fifastreet3.runtime.toml') (Join-Path $Out 'fifastreet3.toml') -Force
Set-Content -Path (Join-Path $Out 'portable.txt') -Value '' -NoNewline
New-Item -ItemType Directory -Force (Join-Path $Out 'tools\deck'), (Join-Path $Out 'tests\input') | Out-Null
Copy-Item (Join-Path $root 'tools\deck\*.sh') (Join-Path $Out 'tools\deck') -Force
Copy-Item (Join-Path $root 'tests\input\*.txt') (Join-Path $Out 'tests\input') -Force
# The Deck runs shell scripts with LF endings.
Get-ChildItem (Join-Path $Out 'tools\deck') -Filter *.sh | ForEach-Object {
  $t = [IO.File]::ReadAllText($_.FullName) -replace "`r`n", "`n"; [IO.File]::WriteAllText($_.FullName, $t)
}
$size = (Get-ChildItem $Out -Recurse -File | Measure-Object Length -Sum).Sum / 1MB
"package: $Out ({0:N0} MB, VC++ runtime from {1})" -f $size, $crt.Parent.Parent.Name
