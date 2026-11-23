# Launch fifastreet3.exe, capture its window at the given times (seconds), then close it.
# Usage: tools\snap.ps1 [-At 15,30,45] [-OutDir work\snaps] [-Prefix run] [-NoLaunch]
#
# Lessons (2026-11-23): PrintWindow and SetForegroundWindow block on the game's window (they wait
# for its window thread), so the window is raised with an async SetWindowPos and the screen is
# copied. The script must be DPI-aware, or window rects and screen coordinates disagree.
param([int[]]$At = @(15, 30, 45), [string]$OutDir = "$PSScriptRoot\..\work\snaps", [string]$Prefix = 'snap',
      [switch]$NoLaunch)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class SnapWin {
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
  public struct RECT { public int Left, Top, Right, Bottom; }
  public struct POINT { public int X, Y; }
}
"@
[SnapWin]::SetProcessDPIAware() | Out-Null
New-Item -ItemType Directory -Force $OutDir | Out-Null
$OutDir = (Resolve-Path $OutDir).Path
$bin = (Resolve-Path (Join-Path $PSScriptRoot '..\out\build\win-amd64-relwithdebinfo')).Path
if ($NoLaunch) { $p = Get-Process fifastreet3 | Select-Object -First 1 }
else { $p = Start-Process (Join-Path $bin 'fifastreet3.exe') -WorkingDirectory $bin -PassThru }
$start = if ($NoLaunch) { $p.StartTime } else { Get-Date }
"pid $($p.Id)"
$HWND_TOPMOST = [IntPtr](-1); $HWND_NOTOPMOST = [IntPtr](-2)
$SWP_NOSIZE_NOMOVE_NOACTIVATE_ASYNC = 0x0001 -bor 0x0002 -bor 0x0010 -bor 0x4000
try {
  foreach ($t in $At) {
    while (((Get-Date) - $start).TotalSeconds -lt $t -and -not $p.HasExited) { Start-Sleep -Milliseconds 250 }
    if ($p.HasExited) { "exited before ${t}s with code $($p.ExitCode)"; break }
    $p.Refresh(); $h = $p.MainWindowHandle
    if ($h -eq [IntPtr]::Zero) { "${t}s: no window yet"; continue }
    [SnapWin]::SetWindowPos($h, $HWND_TOPMOST, 0, 0, 0, 0, $SWP_NOSIZE_NOMOVE_NOACTIVATE_ASYNC) | Out-Null
    Start-Sleep -Milliseconds 400
    $r = New-Object SnapWin+RECT; [SnapWin]::GetClientRect($h, [ref]$r) | Out-Null
    $o = New-Object SnapWin+POINT; [SnapWin]::ClientToScreen($h, [ref]$o) | Out-Null
    $w = $r.Right; $hh = $r.Bottom
    if ($w -le 0 -or $hh -le 0) { "${t}s: window has no size (minimised?)"; continue }
    $bmp = New-Object System.Drawing.Bitmap $w, $hh
    $g = [System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($o.X, $o.Y, 0, 0, $bmp.Size); $g.Dispose()
    [SnapWin]::SetWindowPos($h, $HWND_NOTOPMOST, 0, 0, 0, 0, $SWP_NOSIZE_NOMOVE_NOACTIVATE_ASYNC) | Out-Null
    $file = Join-Path $OutDir "${Prefix}_${t}s.png"; $bmp.Save($file); $bmp.Dispose()
    "${t}s: saved $file (${w}x${hh})"
  }
} finally {
  if (-not $NoLaunch -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force; "stopped after $([int]((Get-Date) - $start).TotalSeconds)s" }
}
