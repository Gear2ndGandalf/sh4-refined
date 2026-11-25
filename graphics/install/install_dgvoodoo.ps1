# SH4 Refined graphics (merged from SH4 Graphics Plus): install dgVoodoo2 2.87.6 as GOG's d3d9
# (D3D8 -> GOG wrapper -> dgVoodoo2 D3D9 -> Direct3D 11, flip discard by default).
# Copies exactly two files into the game folder: d3d9.dll and dgVoodoo.conf. Nothing else is touched.
# Both are checked before either is copied; if anything fails after a copy, the files this run created are removed.
# d3d9.dll must sit next to this script: it is MS\x86\D3D9.dll from dgVoodoo2_87_6.zip
# (..\third_party\dgVoodoo2-2.87.6\, see SOURCE.txt), renamed. The md5 pin refuses any other file.
# v3.40 LG340 (graphics\README.md condition B): the default conf is variants\d3d11_flipdiscard.conf ("Direct3D 11",
# md5 702E9E0A), not the d3d12 one (install\dgVoodoo.conf, md5 67FDE0CF, which stays the d3d12 row's file of
# variants\variants.csv). -Variant <name> installs another pinned row of variants.csv instead. A dgVoodoo.conf that is
# already one of our pinned confs is kept as it is (the player's choice; switch_dgvoodoo_conf.ps1 or the Renderer row
# change it); any other dgVoodoo.conf or d3d9.dll stops the install before anything is copied.
# Usage: powershell -ExecutionPolicy Bypass -File install_dgvoodoo.ps1 [-Game "C:\Program Files (x86)\GOG Galaxy\Games\Silent Hill 4"] [-Variant d3d11_flipdiscard] [-WhatIf]
# Rollback: uninstall_dgvoodoo.ps1 (sends both files to the Recycle Bin).
param(
    [string]$Game = 'C:\Program Files (x86)\GOG Galaxy\Games\Silent Hill 4',
    [string]$Variant = 'd3d11_flipdiscard',
    [switch]$WhatIf
)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
# The default conf's pin is also written here, so an edited variants.csv cannot change what the default installs.
$defaultPin = '702E9E0AF46385A7B59091D5ACA3C404'   # d3d11_flipdiscard: stock conf + OutputAPI d3d11_fl11_0, PresentationModel flip_discard, FullScreenMode false, VRAM 2048, watermark off
$rows = Import-Csv (Join-Path $here 'variants\variants.csv')
$row = $rows | Where-Object name -eq $Variant
if (-not $row) { throw "unknown variant '$Variant' (one of: $($rows.name -join ', '))" }
if ($Variant -eq 'd3d11_flipdiscard' -and $row.md5 -ne $defaultPin) { throw "variants.csv pins d3d11_flipdiscard as $($row.md5), expected $defaultPin" }
$confPins = @($rows.md5)   # every pinned conf (d3d12 = install\dgVoodoo.conf included)
# file name in the game folder -> (source next to this script, its pin)
$pins = [ordered]@{
    'd3d9.dll'      = @('d3d9.dll', 'CAA75C55D9B3121D17433BD3B38C7BB2')   # dgVoodoo2 2.87.6 MS\x86\D3D9.dll
    'dgVoodoo.conf' = @($row.file, $row.md5)
}
# 1. The game folder must be the GOG v7 install, and the game must be closed.
$exe = Join-Path $Game 'SILENT HILL 4.exe'
if (-not (Test-Path $exe)) { throw "no SILENT HILL 4.exe in $Game" }
if (-not (Test-Path (Join-Path $Game 'ori_d3d8.dll')) -and -not (Test-Path (Join-Path $Game 'd3d8.dll'))) { throw 'GOG d3d8 wrapper not found' }
if (Get-Process -Name 'SILENT HILL 4' -ErrorAction SilentlyContinue) { throw 'close the game first' }
# 2. Preflight everything before any copy: our sources are the pinned files, and each destination is absent or
#    already ours (never overwrite another tool's d3d9.dll / dgVoodoo.conf).
$todo = @()
foreach ($f in $pins.Keys) {
    $src = Join-Path $here $pins[$f][0]
    $pin = $pins[$f][1]
    if (-not (Test-Path $src)) { throw "missing $src (extract MS\x86\D3D9.dll from the dgVoodoo2 2.87.6 zip as d3d9.dll)" }
    $h = (Get-FileHash $src -Algorithm MD5).Hash
    if ($h -ne $pin) { throw "$src is $h, expected $pin" }
    $dst = Join-Path $Game $f
    if (Test-Path $dst) {
        $h = (Get-FileHash $dst -Algorithm MD5).Hash
        if ($f -eq 'dgVoodoo.conf' -and $confPins -contains $h) {
            $cur = ($rows | Where-Object md5 -eq $h | Select-Object -First 1).name
            if ($h -eq $pin) { "already installed: $f ($Variant)" }
            else { "already installed: $f is our $cur conf: kept (switch_dgvoodoo_conf.ps1 -Variant $Variant or the Renderer row changes it)" }
            continue
        }
        if ($h -ne $pin) { throw "$dst exists with md5 $h (not ours): remove or back it up first; nothing was copied" }
        "already installed: $f"
    } else {
        $todo += $f
    }
}
if (-not $todo.Count) { 'done (nothing to copy).'; return }
if ($WhatIf) { $todo | ForEach-Object { "would copy $_ (from $($pins[$_][0]), md5 $($pins[$_][1]))" }; return }
# 3. Copy (no-overwrite: fails if the file appeared since the preflight), verify, and undo this run's copies on any
#    failure, but only files that still hold our pinned bytes (never someone else's file).
$created = @()
try {
    foreach ($f in $todo) {
        $dst = Join-Path $Game $f
        $pin = $pins[$f][1]
        [System.IO.File]::Copy((Join-Path $here $pins[$f][0]), $dst, $false)   # overwrite = false (review SGP-06)
        $created += [pscustomobject]@{ Path = $dst; Pin = $pin }
        $h = (Get-FileHash $dst -Algorithm MD5).Hash
        if ($h -ne $pin) { throw "copy of $f failed verification ($h)" }
        "installed: $f ($h)"
    }
} catch {
    # Rollback goes to the Recycle Bin (review SGP-09): even if another program swapped a file in the moment between the
    # hash check and the delete, nothing is lost for good.
    Add-Type -AssemblyName Microsoft.VisualBasic
    foreach ($c in $created) {
        if ((Test-Path -LiteralPath $c.Path) -and (Get-FileHash -LiteralPath $c.Path -Algorithm MD5).Hash -eq $c.Pin) {
            [Microsoft.VisualBasic.FileIO.FileSystem]::DeleteFile($c.Path, 'OnlyErrorDialogs', 'SendToRecycleBin'); "rolled back (Recycle Bin): $($c.Path)"
        } elseif (Test-Path -LiteralPath $c.Path) { "left alone (not our bytes): $($c.Path)" }
    }
    throw
}
"done ($Variant). Start the game; SH4Refined.log's renderer line names the conf, the display line the Direct3D runtime."
