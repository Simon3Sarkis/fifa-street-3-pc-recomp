# Build the Windows release zip: dist\fifastreet3-<version>-windows-x64.zip
# Usage: & .\tools\make_release.ps1 [-Version 0.0.2]
# Contents: the exe, runtime + GPU plugin DLLs, the VC++ runtime (app-local), fifastreet3.toml (the
# shipped settings), recomplauncher.json, portable.txt (saves and logs stay next to the exe),
# README.txt, fonts\ (Anton, SIL OFL 1.1, for the menus), the project licence and every bundled
# library's licence. No game files, no test tools.
# The player's own choices go to fifastreet3.user.toml, which a release never contains, so unpacking
# a new version over an old one keeps them.
param([string]$Version = '0.0.2')

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$bin = Join-Path $root 'out\build\win-amd64-relwithdebinfo'
# The SDK writes its DLLs to its own out folder; the copies next to the exe go stale when only a DLL
# changed.
$sdkBin = Join-Path $root 'third_party\rexglue-sdk\out\win-amd64'
$crt = Get-ChildItem 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Redist\MSVC\*\x64\Microsoft.VC143.CRT' -Directory |
  Sort-Object FullName | Select-Object -Last 1
if (-not $crt) { throw 'VC++ redist (Microsoft.VC143.CRT) not found' }

$name = "fifastreet3-$Version-windows-x64"
$out = Join-Path $root "dist\$name"
if (Test-Path $out) { Remove-Item -Recurse -Force $out }
New-Item -ItemType Directory -Force $out | Out-Null

Copy-Item (Join-Path $bin 'fifastreet3.exe') $out
Get-ChildItem $sdkBin -Filter *.dll | Copy-Item -Destination $out
foreach ($d in 'msvcp140.dll', 'msvcp140_atomic_wait.dll', 'vcruntime140.dll', 'vcruntime140_1.dll') {
  Copy-Item (Join-Path $crt.FullName $d) $out
}
Copy-Item (Join-Path $root 'config\fifastreet3.runtime.toml') (Join-Path $out 'fifastreet3.toml')
Copy-Item (Join-Path $root 'packaging\recomplauncher.json') $out
Copy-Item (Join-Path $root 'packaging\README.txt') $out
New-Item -ItemType Directory -Force (Join-Path $out 'fonts') | Out-Null
Copy-Item (Join-Path $root 'packaging\fonts\Anton-Regular.ttf') (Join-Path $out 'fonts')
Set-Content -Path (Join-Path $out 'VERSION') -Value $Version -NoNewline
Set-Content -Path (Join-Path $out 'portable.txt') -Value '' -NoNewline

# Licences: the project's (GPL-3), the SDK's (BSD-3) and every bundled library's.
$lic = Join-Path $out 'licenses'
New-Item -ItemType Directory -Force $lic | Out-Null
Copy-Item (Join-Path $root 'LICENSE') (Join-Path $lic 'fifastreet3-pc-port-GPL-3.0.txt')
Copy-Item (Join-Path $root 'third_party\rexglue-sdk\LICENSE') (Join-Path $lic 'rexglue-sdk.txt')
Copy-Item (Join-Path $root 'packaging\fonts\OFL.txt') (Join-Path $lic 'Anton-font-OFL-1.1.txt')
$tp = Join-Path $root 'third_party\rexglue-sdk\thirdparty'
foreach ($dir in Get-ChildItem $tp -Directory) {
  $files = @(Get-ChildItem $dir.FullName -File | Where-Object { $_.Name -match '^(license|copying|copyright|notice|unlicense)' })
  if (-not $files) {
    $files = @(Get-ChildItem $dir.FullName -File -Recurse -Depth 1 -ErrorAction SilentlyContinue |
      Where-Object { $_.Name -match '^(license|copying|copyright|unlicense)' } | Select-Object -First 3)
  }
  foreach ($f in $files) {
    Copy-Item $f.FullName (Join-Path $lic ("{0}-{1}" -f $dir.Name, $f.Name))
  }
}

$zip = Join-Path $root "dist\$name.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path (Join-Path $out '*') -DestinationPath $zip
$size = (Get-Item $zip).Length / 1MB
"release: $zip ({0:N1} MB, VC++ runtime {1})" -f $size, $crt.Parent.Parent.Name
