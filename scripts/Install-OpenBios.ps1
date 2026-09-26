param([Parameter(Mandatory=$true)][string]$CardRoot)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$card=(Resolve-Path -LiteralPath $CardRoot).Path
$source=(Resolve-Path -LiteralPath (Join-Path $repo 'assets/open-bios')).Path
$hashes=@{
 'ps1/openbios.bin'='FABE498FBF224E4721F12F31B6F5FE0659205E341DC4E5C5F91B9BD1A1011C57'
 'gba/open_gba_bios.bin'='661A9AFB93624F2C5E77D07DAB137BD8D23CFF79E8639C62FE621B49BB064749'
 'sameboy/dmg_boot.bin'='6F64DA4CECD7E54E2F928EB3E3BA7810A7A567D0D247CC71737D1771E073A916'
 'sameboy/cgb_boot.bin'='F767B8E7E510A255F81328C89DBA6E0C996B370E1BC86AEBB8584A7DA47A5BBA'
}
foreach($rel in $hashes.Keys){
 if((Get-FileHash -LiteralPath (Join-Path $source $rel)).Hash -ne $hashes[$rel]){throw "Firmware source hash mismatch: $rel"}
}
$copies=@(Get-ChildItem -LiteralPath $source -File -Recurse | ForEach-Object {
 [pscustomobject]@{Source=$_.FullName;Dest=(Join-Path $card ('cubegm/bios/open-source/'+$_.FullName.Substring($source.Length+1)))}
})
$copies += [pscustomobject]@{Source=(Join-Path $repo 'docs/r36sx/BIOS-GUIDE.md');Dest=(Join-Path $card 'BIOS-GUIDE.md')}
$copies += [pscustomobject]@{Source=(Join-Path $repo 'docs/r36sx/BIOS-GUIDE.md');Dest=(Join-Path $card 'docs/BIOS-GUIDE.md')}
# Preflight all destinations before writing anything. Existing private BIOS,
# active firmware aliases and per-game configuration are never touched.
foreach($copy in $copies){
 if((Test-Path -LiteralPath $copy.Dest) -and
    (Get-FileHash -LiteralPath $copy.Source).Hash -ne (Get-FileHash -LiteralPath $copy.Dest).Hash){
  throw "Destination differs; preserve it and resolve manually: $($copy.Dest)"
 }
}
foreach($copy in $copies){
 $parent=Split-Path -Parent $copy.Dest
 if(-not(Test-Path -LiteralPath $parent)){New-Item -ItemType Directory -Force -Path $parent | Out-Null}
 if(-not(Test-Path -LiteralPath $copy.Dest)){Copy-Item -LiteralPath $copy.Source -Destination $copy.Dest}
 if((Get-FileHash -LiteralPath $copy.Source).Hash -ne (Get-FileHash -LiteralPath $copy.Dest).Hash){throw 'Installed firmware hash mismatch'}
}
Write-Host "Verified $($copies.Count) firmware/source/license/guide files; active BIOS/configuration preserved."
