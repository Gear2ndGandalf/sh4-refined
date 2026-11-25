# SH4 Graphics Plus: switch the installed dgVoodoo.conf to another pinned variant (restart-only settings).
# Variants and their md5s: variants\variants.csv (d3d11_flipdiscard = the default since v3.40 LG340, d3d11fd_rt16, d3d12
# = install\dgVoodoo.conf (the default up to v3.39), passthru, d3d11, d3d12_10bit, d3d12_hdr, d3d11fd_rt10).
# Replaces the game folder's dgVoodoo.conf only if it is one of OUR pinned confs (never a hand-edited or foreign one);
# the old conf goes to the Recycle Bin. Needs dgVoodoo2 installed (install_dgvoodoo.ps1) and the game closed.
# Take the SESSIONS_BOARD game-folder lock first. Usage:
#   powershell -ExecutionPolicy Bypass -File switch_dgvoodoo_conf.ps1 -Variant d3d11 [-Game "C:\Program Files (x86)\GOG Galaxy\Games\Silent Hill 4"] [-WhatIf]
param(
    [Parameter(Mandatory)][string]$Variant,
    [string]$Game = 'C:\Program Files (x86)\GOG Galaxy\Games\Silent Hill 4',
    [switch]$WhatIf
)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$rows = Import-Csv (Join-Path $here 'variants\variants.csv')
$row = $rows | Where-Object name -eq $Variant
if (-not $row) { throw "unknown variant '$Variant' (one of: $($rows.name -join ', '))" }
if (-not (Test-Path (Join-Path $Game 'SILENT HILL 4.exe'))) { throw "no SILENT HILL 4.exe in $Game" }
if (Get-Process -Name 'SILENT HILL 4' -ErrorAction SilentlyContinue) { throw 'close the game first' }
$dll = Join-Path $Game 'd3d9.dll'
if (-not (Test-Path $dll) -or (Get-FileHash $dll -Algorithm MD5).Hash -ne 'CAA75C55D9B3121D17433BD3B38C7BB2') { throw 'dgVoodoo2 2.87.6 is not installed here (run install_dgvoodoo.ps1 first)' }
$src = Join-Path $here $row.file
if ((Get-FileHash $src -Algorithm MD5).Hash -ne $row.md5) { throw "$src does not match its pin $($row.md5)" }
$dst = Join-Path $Game 'dgVoodoo.conf'
if (Test-Path $dst) {
    $h = (Get-FileHash $dst -Algorithm MD5).Hash
    $cur = $rows | Where-Object md5 -eq $h
    if (-not $cur) { throw "$dst has md5 $h, not one of our pinned confs: back it up or remove it first; nothing was changed" }
    if ($cur.name -eq $Variant) { "already on $Variant"; return }
    if ($WhatIf) { "would switch $($cur.name) -> $Variant"; return }
    Add-Type -AssemblyName Microsoft.VisualBasic
    [Microsoft.VisualBasic.FileIO.FileSystem]::DeleteFile($dst, 'OnlyErrorDialogs', 'SendToRecycleBin')
    "recycled the $($cur.name) conf"
} elseif ($WhatIf) { "would copy the $Variant conf"; return }
[System.IO.File]::Copy($src, $dst, $false)   # no overwrite: fails if a file appeared meanwhile
$h = (Get-FileHash $dst -Algorithm MD5).Hash
if ($h -ne $row.md5) { throw "copy failed verification ($h)" }
"now on $Variant ($h). Restart the game for it to apply."
