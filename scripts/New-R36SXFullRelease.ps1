param(
    [Parameter(Mandatory=$true)][string]$TestedCardRoot,
    [Parameter(Mandatory=$true)][string]$StockCardRoot,
    [Parameter(Mandatory=$true)][ValidateSet('2.6','2.7')][string]$BoardRevision,
    [Parameter(Mandatory=$true)][string]$StockPhotoRoot,
    [string]$Version='1.4.0',
    [string]$OutputDirectory=(Join-Path $PSScriptRoot '../dist')
)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$card=(Resolve-Path -LiteralPath $TestedCardRoot).Path.TrimEnd('\')
$stock=(Resolve-Path -LiteralPath $StockCardRoot).Path.TrimEnd('\')
$out=[IO.Path]::GetFullPath($OutputDirectory)
$name="SwitchFrogUI-$Version-R36SX-v$BoardRevision"
$stage=Join-Path $out $name
if(Test-Path -LiteralPath $stage){throw "Stage already exists: $stage. Use a fresh output directory."}
New-Item -ItemType Directory -Path $stage -Force | Out-Null
function Copy-One([string]$Source,[string]$Relative){
    if(-not(Test-Path -LiteralPath $Source -PathType Leaf)){throw "Missing required file: $Source"}
    $dest=Join-Path $stage $Relative
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dest) | Out-Null
    Copy-Item -LiteralPath $Source -Destination $dest -Force
}
function Copy-Tree([string]$Source,[string]$Relative){
    if(-not(Test-Path -LiteralPath $Source -PathType Container)){throw "Missing runtime directory: $Source"}
    $resolved=(Resolve-Path -LiteralPath $Source).Path.TrimEnd('\')
    Get-ChildItem -LiteralPath $resolved -Recurse -File -Force | ForEach-Object {
        $tail=$_.FullName.Substring($resolved.Length+1)
        if($tail -match '(?i)(^|[\\/])(bios|saves?|states?)([\\/]|$)|\.pre-|\.log$|\.sav$|\.mcr$|\.zip$|gba_bios|neogeo|pgm\.zip|(^|[\\/])\.ash_history$'){return}
        Copy-One $_.FullName (Join-Path $Relative $tail)
    }
}

# Complete board-specific boot system, not an overlay. Keep kernel/DTB aliases
# together: stock boots through these unexpectedly named files too.
Copy-Tree (Join-Path $stock 'rootfs') 'rootfs'
Copy-Tree (Join-Path $stock 'cubegm/usr') 'cubegm/usr'
Copy-Tree (Join-Path $stock 'cubegm/lib') 'cubegm/lib'
Get-ChildItem -LiteralPath (Join-Path $stock 'cubegm') -File | Where-Object {
    $_.Name -match '^(advapi32\.dll|ApplicationFrame\.dll|Bubbles\.scr|dtb\.bin|.*\.uImage|.*\.cpd|.*\.ttf|.*\.wav|cubepoweroff\.bmp|xgame-logo1\.hc|driver\.so|rkgame|icube|icube\.sh|icube_start\.sh|icubemp_start\.sh|MyExecutable|AcXtrnal|root\.dat|resource|pagefile\.sys)$'
} | ForEach-Object {Copy-One $_.FullName (Join-Path 'cubegm' $_.Name)}

$runtime=@('picoarch','picoarch_hi','pcsx4all','rockbox','rockbox.sh','video_player',
 'video_player.sh','ebook','nosleep','r36sx_displayfix.so','driver_r36sx.so',
 'driver_r36sx27.so','zhijack.sh','setting.xml','xgame-logo.bmp','switchfrog-update.sh',
 'pico286','lgpt','lgpt.elf')
foreach($file in $runtime){Copy-One (Join-Path $card "cubegm/$file") "cubegm/$file"}
Copy-One (Join-Path $card 'cubegm/cores/libemu_md.so') 'cubegm/cores/libemu_md.so'
Get-ChildItem -LiteralPath (Join-Path $card 'cubegm/cores') -File -Filter '*_libretro.so' |
    ForEach-Object {Copy-One $_.FullName (Join-Path 'cubegm/cores' $_.Name)}
foreach($file in @('libSDL-1.2.so.0','libpng12.so.0','libpng16.so.16','libz.so.1')){
    Copy-One (Join-Path $card "cubegm/lib/$file") "cubegm/lib/$file"
}
# Static frontend assets only; never copy a user's settings/keymap/history.
Get-ChildItem -LiteralPath (Join-Path $card 'frogui') -File | Where-Object {
    $_.Extension -match '^\.(jpg|jpeg|png|bmp)$' -and $_.Name -notmatch '\.pre-'
} | ForEach-Object {Copy-One $_.FullName (Join-Path 'frogui' $_.Name)}
foreach($dir in @('fonts','sounds','wallpapers','icon-packs','theme-packs')){
    $source=Join-Path $card "frogui/$dir"
    if(Test-Path -LiteralPath $source){Copy-Tree $source "frogui/$dir"}
}
foreach($file in @('keymap.txt','keyboard_gamepad.cfg')){
    Copy-One (Join-Path $repo "assets/config/$file") "frogui/$file"
}
Copy-One (Join-Path $repo 'assets/config/release-settings.txt') 'frogui/settings.txt'
Copy-One (Join-Path $repo 'assets/config/release-picoarch.cfg') 'picoarch.cfg'
Copy-One (Join-Path $card 'SwitchFrogUI BUILD.txt') 'SwitchFrogUI BUILD.txt'
Copy-One (Join-Path $card 'MD/dummy.md') 'MD/dummy.md'
Copy-One (Join-Path $card 'MD/filelist.csv') 'MD/filelist.csv'

# Rockbox's hidden application directory is required even though it is under roms.
# Use explicit directories and factory config to avoid resume/database/history.
$rb=Join-Path $card 'roms/rockbox/.rockbox'
foreach($dir in @('backdrops','codecs','codepages','docs','eqs','fonts','icons','langs','rocks','themes','wps')){
    Copy-Tree (Join-Path $rb $dir) "roms/rockbox/.rockbox/$dir"
}
foreach($file in @('rockbox','rockbox-info.txt','tagnavi.config','viewers.config','database.ignore')){
    Copy-One (Join-Path $rb $file) "roms/rockbox/.rockbox/$file"
}
Copy-One (Join-Path $repo 'apps/rockbox-config.cfg') 'roms/rockbox/.rockbox/config.cfg'
Copy-One (Join-Path $repo 'apps/rockbox-shortcuts.txt') 'roms/rockbox/.rockbox/shortcuts.txt'

# The requested, explicit sample allow-list is the only media included.
foreach($file in @('sample.mp4','SwitchFrogUI Video Test.mp4','SwitchFrogUI Video Test.srt')){
    Copy-One (Join-Path $card "Videos/$file") "Videos/$file"
}
Copy-One (Join-Path $repo 'apps/assets/SwitchFrogUI Sample - Mozart - Piano Sonata No. 14.ogg') 'Music/SwitchFrogUI Samples/Mozart - Piano Sonata No. 14.ogg'
Copy-One (Join-Path $repo 'apps/assets/ebooks/World English Bible (WEB).epub') 'Ebooks/World English Bible (WEB).epub'
Copy-One (Join-Path $repo 'apps/assets/ebooks/SOURCES.md') 'Ebooks/SOURCES.md'
Get-ChildItem -LiteralPath (Join-Path $repo 'apps/jsdev/examples') -File -Filter '*.js' |
    ForEach-Object {Copy-One $_.FullName (Join-Path 'roms/JSDev' $_.Name)}
foreach($file in @('photo (1).jpg','photo (2).jpg','photo (3).jpg')){
    Copy-One (Join-Path $StockPhotoRoot $file) "Photo/$file"
}
foreach($dir in @('gba','gb','gbc','nes','snes','ps1','md','arcade','doom','heretic','hexen','Ebook')){
    New-Item -ItemType Directory -Force -Path (Join-Path $stage "roms/$dir") | Out-Null
}
foreach($file in @('LICENSE.md','release-notes.md','cores.md')){Copy-One (Join-Path $repo $file) $file}
Copy-One (Join-Path $repo 'cores.md') 'CORE-SOURCES.md'
foreach($file in @('INSTALL.md','FEATURES.md','THIRD_PARTY_NOTICES.md','JSDEV.md')){
    Copy-One (Join-Path $repo "docs/r36sx/$file") "docs/$file"
}
$utf8=New-Object Text.UTF8Encoding($false)
[IO.File]::WriteAllText((Join-Path $stage 'START-HERE.txt'),
 "SwitchFrogUI $Version - R36SX motherboard v$BoardRevision`nExtract ALL ZIP contents directly to an empty FAT32 card root, including rootfs and hidden .rockbox files.`nNo separate stock download is required. ROMs and console BIOS files are not included.`nDefault theme: Ocean Depth Gradient.`nOnly R36SX v2.7 has been hardware-tested during development. v2.6 is experimental and not guaranteed.`nNew 1.4.0 changes still need device testing. Imported TreeFrogUI features have not all been thoroughly tested.`nSee docs/INSTALL.md and release-notes.md.`n",$utf8)
[IO.File]::WriteAllText((Join-Path $stage 'BOARD.txt'),"R36SX motherboard v$BoardRevision`nSwitchFrogUI $Version`n",$utf8)

# Check required boot/application files, forbidden payloads and fresh defaults.
$required=@('rootfs/etc/init.d/rcS','cubegm/rkgame','cubegm/dtb.bin','cubegm/vmlinux.uImage',
 'cubegm/advapi32.dll','cubegm/ApplicationFrame.dll','cubegm/Bubbles.scr',
 'cubegm/cores/libemu_md.so','cubegm/cores/frogui_libretro.so','cubegm/picoarch',
 'cubegm/picoarch_hi','cubegm/pcsx4all','cubegm/ebook','cubegm/rockbox','cubegm/video_player',
 'roms/rockbox/.rockbox/rockbox','MD/dummy.md','MD/filelist.csv')
foreach($file in $required){if(-not(Test-Path -LiteralPath (Join-Path $stage $file))){throw "Missing boot/runtime file: $file"}}
$files=Get-ChildItem -LiteralPath $stage -Recurse -File -Force
foreach($file in $files){
    $rel=$file.FullName.Substring($stage.Length+1).Replace('\','/')
    if($rel -match '(?i)(^|/)(bios|saves?|states?)(/|$)|\.pre-|favorites|game_history|playtime|last_game|home_selection|locked_directories|\.log$|\.sav$|\.mcr$|\.st[0-9]+$|gba_bios|scph[0-9]|neogeo\.zip|pgm\.zip' -or
       $file.Extension -match '(?i)^\.(nes|gba|gb|gbc|sfc|smc|chd|cue|iso|pbp|z64|n64|wad|srm)$' -or
       ($rel -match '^roms/' -and $rel -notmatch '^roms/(rockbox/\.rockbox/|JSDev/[^/]+\.js$)')){
        throw "Release audit rejected content: $rel"
    }
}
$hashes=$files | Sort-Object FullName | ForEach-Object {
    $hash=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    "$hash  $($_.FullName.Substring($stage.Length+1).Replace('\','/'))"
}
[IO.File]::WriteAllLines((Join-Path $stage 'SHA256SUMS.txt'),$hashes,$utf8)
[IO.File]::WriteAllText((Join-Path $stage 'PUBLISH-AUDIT.txt'),
 "PASS: complete board-specific boot/runtime checks`nPASS: ROM/BIOS/save/history exclusions`nPASS: explicit requested media allow-list`nFiles: $($files.Count)`nBoard: $BoardRevision`n",$utf8)
# .NET ZipFile includes hidden .rockbox content; Compress-Archive may omit it.
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip=Join-Path $out "$name.zip"
[IO.Compression.ZipFile]::CreateFromDirectory($stage,$zip,[IO.Compression.CompressionLevel]::Optimal,$false)
$archive=[IO.Compression.ZipFile]::OpenRead($zip)
try {
    foreach($file in $required){if(-not $archive.GetEntry($file)){throw "ZIP missing required entry $file"}}
    if($archive.Entries.Count -lt $files.Count){throw 'ZIP lost files during creation'}
} finally {$archive.Dispose()}
$hash=(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash
[IO.File]::WriteAllText("$zip.sha256","$hash  $name.zip`n",$utf8)
Write-Host "Verified full-card release: $zip"
