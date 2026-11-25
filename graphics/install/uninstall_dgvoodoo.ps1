# SH4 Graphics Plus: remove dgVoodoo2 from the game folder (back to GOG's wrapper on the system d3d9).
# Sends d3d9.dll and dgVoodoo.conf to the Recycle Bin, but only the files whose md5 is ours.
# Usage: powershell -ExecutionPolicy Bypass -File uninstall_dgvoodoo.ps1 [-Game "C:\Program Files (x86)\GOG Galaxy\Games\Silent Hill 4"] [-WhatIf]
param(
    [string]$Game = 'C:\Program Files (x86)\GOG Galaxy\Games\Silent Hill 4',
    [switch]$WhatIf
)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
# dgVoodoo.conf counts as ours if it is any pinned variant (variants\variants.csv). v3.40 LG340: the default is now
# d3d11_flipdiscard (install_dgvoodoo.ps1); every conf ever pinned is also listed here, so a conf installed by an
# older script is still recognised even if variants.csv is missing or lost a row.
$builtIn = @(
    '67FDE0CF1FA6E95EFB53F0F398540F4E',   # d3d12 (install\dgVoodoo.conf, the default up to v3.39)
    '702E9E0AF46385A7B59091D5ACA3C404',   # d3d11_flipdiscard (the default since v3.40)
    '6643C79F839016623A083B687C4FAC34',   # d3d11
    'A38A9DBEDA5F91B8CAD8B7836F05A885',   # d3d12_10bit
    '4FFF0B9E29A63CA772F9E7FE8FE15E5D',   # d3d12_hdr
    'C500F5C9E089D915288AD20FF89CD68A',   # passthru ("Stock")
    '94912DBFE6D57EB23F47A1B8DC13A893',   # d3d11fd_rt10
    'C7E9253A95A07A1466306F417978204A'    # d3d11fd_rt16
)
$csv = Join-Path $here 'variants\variants.csv'
$confPins = $builtIn + @(if (Test-Path $csv) { (Import-Csv $csv).md5 })
# dgVoodoo.conf.new: SH4 Refined's Renderer row writes it before swapping; a leftover one with a pinned md5 is ours too
# (Refined v3.39 would otherwise rename it to dgVoodoo.conf at the next start).
$pins = [ordered]@{
    'dgVoodoo.conf.new' = $confPins
    'd3d9.dll'          = @('CAA75C55D9B3121D17433BD3B38C7BB2')
    'dgVoodoo.conf'     = $confPins
}
if (-not (Test-Path (Join-Path $Game 'SILENT HILL 4.exe'))) { throw "no SILENT HILL 4.exe in $Game" }
if (Get-Process -Name 'SILENT HILL 4' -ErrorAction SilentlyContinue) { throw 'close the game first' }
Add-Type -AssemblyName Microsoft.VisualBasic
foreach ($f in $pins.Keys) {
    $dst = Join-Path $Game $f
    if (-not (Test-Path $dst)) { "not present: $f"; continue }
    $h = (Get-FileHash $dst -Algorithm MD5).Hash
    if ($pins[$f] -notcontains $h) { "left alone: $f (md5 $h is not ours)"; continue }
    if ($WhatIf) { "would recycle $f"; continue }
    [Microsoft.VisualBasic.FileIO.FileSystem]::DeleteFile($dst, 'OnlyErrorDialogs', 'SendToRecycleBin')
    "recycled: $f"
}
# The variants folder for SH4 Refined's Renderer row (install_variants_folder.ps1): only our confs and manifest go;
# the folder is removed only when nothing else is left in it.
$dir = Join-Path $Game 'dgVoodoo_variants'
if (Test-Path $dir) {
    foreach ($fi in Get-ChildItem $dir -File) {
        $ours = if ($fi.Name -eq 'manifest.csv') { (Get-Content $fi.FullName -TotalCount 1) -like '# SH4 Graphics Plus*' } else { $pins['dgVoodoo.conf'] -contains (Get-FileHash $fi.FullName -Algorithm MD5).Hash }
        if (-not $ours) { "left alone: dgVoodoo_variants\$($fi.Name) (not ours)"; continue }
        if ($WhatIf) { "would recycle dgVoodoo_variants\$($fi.Name)"; continue }
        [Microsoft.VisualBasic.FileIO.FileSystem]::DeleteFile($fi.FullName, 'OnlyErrorDialogs', 'SendToRecycleBin'); "recycled: dgVoodoo_variants\$($fi.Name)"
    }
    if (-not $WhatIf -and -not (Get-ChildItem $dir -Force)) { Remove-Item $dir; 'removed: dgVoodoo_variants\' }
}
'done.'
