# SH4 Graphics Plus: put the pinned dgVoodoo confs that SH4 Refined's "Renderer" row may choose into the game folder,
# as <game>\dgVoodoo_variants\<name>.conf plus manifest.csv (name,file,md5,label). Refined copies one of them over
# dgVoodoo.conf (rules: the manifest's own comment lines). Additive only: never touches d3d9.dll or dgVoodoo.conf,
# never overwrites a file in the folder that is not ours. Game closed, SESSIONS_BOARD lock first. Usage:
#   powershell -ExecutionPolicy Bypass -File install_variants_folder.ps1 [-Game "C:\Program Files (x86)\GOG Galaxy\Games\Silent Hill 4"] [-WhatIf]
param(
    [string]$Game = 'C:\Program Files (x86)\GOG Galaxy\Games\Silent Hill 4',
    [switch]$WhatIf
)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
# Selectable choices (file + label) in menu order; every other pinned conf is listed md5-only, so Refined still
# recognises it as ours (shown as "Graphics Plus: <name>", replaceable) instead of "Custom".
$choices = [ordered]@{
    'd3d11_flipdiscard' = 'Direct3D 11'
    'd3d11fd_rt16'      = 'Direct3D 11 (16-bit)'
    'd3d12'             = 'Direct3D 12'
    'passthru'          = 'Stock'
}
$rows = Import-Csv (Join-Path $here 'variants\variants.csv')
foreach ($n in $choices.Keys) { if (-not ($rows | Where-Object name -eq $n)) { throw "variant $n is not in variants.csv" } }
if (-not (Test-Path (Join-Path $Game 'SILENT HILL 4.exe'))) { throw "no SILENT HILL 4.exe in $Game" }
if (Get-Process -Name 'SILENT HILL 4' -ErrorAction SilentlyContinue) { throw 'close the game first' }
$dir = Join-Path $Game 'dgVoodoo_variants'
# Check everything before changing anything.
foreach ($n in $choices.Keys) {
    $row = $rows | Where-Object name -eq $n
    $src = Join-Path $here $row.file
    if ((Get-FileHash $src -Algorithm MD5).Hash -ne $row.md5) { throw "$src does not match its pin $($row.md5)" }
    $dst = Join-Path $dir "$n.conf"
    if ((Test-Path $dst) -and (Get-FileHash $dst -Algorithm MD5).Hash -ne $row.md5) { throw "$dst exists and is not our pinned $n conf; nothing was changed" }
}
$man = Join-Path $dir 'manifest.csv'
if ((Test-Path $man) -and (Get-Content $man -TotalCount 1) -notlike '# SH4 Graphics Plus*') { throw "$man exists and is not ours; nothing was changed" }
$lines = @(
    '# SH4 Graphics Plus dgVoodoo conf variants (written by install_variants_folder.ps1; do not edit).',
    '# A program may copy <file> over ..\dgVoodoo.conf only if: the game is not yet drawing with it (it applies at the next start);',
    '# ..\d3d9.dll has an md5 of a #d3d9.dll= line (dgVoodoo2 2.87.6); the current ..\dgVoodoo.conf md5 is one of the md5s below;',
    '# <file> has its md5 below. Write a temporary file, check its md5, then replace dgVoodoo.conf. Rows with an empty file are',
    '# ours but not offered. Lines starting with # are comments, except #d3d9.dll=<md5>: the d3d9.dll md5(s) the confs are',
    '# for (one line each; ..\d3d9.dll must match one of them).',
    '#d3d9.dll=CAA75C55D9B3121D17433BD3B38C7BB2',
    'name,file,md5,label'
)
foreach ($n in $choices.Keys) { $row = $rows | Where-Object name -eq $n; $lines += "$n,$n.conf,$($row.md5),$($choices[$n])" }
foreach ($row in $rows | Where-Object { -not $choices.Contains($_.name) }) { $lines += "$($row.name),,$($row.md5)," }
if ($WhatIf) { "would write $dir with $($choices.Count) confs and a manifest of $($rows.Count) rows:"; $lines; return }
New-Item -ItemType Directory -Force $dir | Out-Null
foreach ($n in $choices.Keys) {
    $row = $rows | Where-Object name -eq $n; $dst = Join-Path $dir "$n.conf"
    if (Test-Path $dst) { "already there: $n.conf"; continue }
    [System.IO.File]::Copy((Join-Path $here $row.file), $dst, $false)
    if ((Get-FileHash $dst -Algorithm MD5).Hash -ne $row.md5) { throw "copy of $n failed verification" }
    "copied: $n.conf ($($row.md5))"
}
[System.IO.File]::WriteAllText($man, (($lines -join "`r`n") + "`r`n"), [System.Text.Encoding]::ASCII)
"manifest: $man ($($rows.Count) pinned confs, $($choices.Count) offered)"
