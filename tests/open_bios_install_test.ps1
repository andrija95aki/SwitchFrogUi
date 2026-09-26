$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$card=(New-Item -ItemType Directory -Path (Join-Path ([IO.Path]::GetTempPath()) ('switchfrog-bios-test-'+[guid]::NewGuid()))).FullName
$bios=New-Item -ItemType Directory -Path (Join-Path $card 'cubegm/bios') -Force
$active=Join-Path $bios.FullName 'gba_bios.bin'
[IO.File]::WriteAllText($active,'existing user firmware test fixture')
$before=(Get-FileHash $active).Hash
& (Join-Path $repo 'scripts/Install-OpenBios.ps1') -CardRoot $card
& (Join-Path $repo 'scripts/Install-OpenBios.ps1') -CardRoot $card
if((Get-FileHash $active).Hash -ne $before){throw 'Active BIOS was overwritten'}
foreach($file in @('ps1/openbios.bin','gba/open_gba_bios.bin','sameboy/dmg_boot.bin','sameboy/cgb_boot.bin','gpsp-source/COPYING','licenses/OpenBIOS.txt','licenses/SameBoy.txt')){
 if(-not(Test-Path -LiteralPath (Join-Path $card "cubegm/bios/open-source/$file"))){throw "Missing file: $file"}
}
[IO.File]::WriteAllText((Join-Path $card 'BIOS-GUIDE.md'),'user-customized guide')
$refused=$false
try{& (Join-Path $repo 'scripts/Install-OpenBios.ps1') -CardRoot $card}catch{$refused=$true}
if(-not $refused){throw 'Conflicting file was not protected'}
if([IO.File]::ReadAllText((Join-Path $card 'BIOS-GUIDE.md')) -ne 'user-customized guide'){throw 'Conflicting guide changed'}
Write-Host 'PASS: firmware hashes/licenses, repeat install, active BIOS preservation, collision refusal.'
