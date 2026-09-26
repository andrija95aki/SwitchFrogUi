param([Parameter(Mandatory=$true)][string]$CardRoot)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$card=(Resolve-Path -LiteralPath $CardRoot).Path
$source=[IO.File]::ReadAllText((Join-Path $repo 'frogui/frogui_libretro.c'))
$table=[regex]::Match($source,'(?s)static const ConsoleMapping console_mappings\[\] = \{(.*?)\n\};')
if(-not $table.Success){throw 'Cannot locate the frontend console mappings'}
$folders=@{}
foreach($item in [regex]::Matches($table.Groups[1].Value,'\{"([^"]+)",\s*CORES_PATH "/([^"]+)"\}')){
    $name=$item.Groups[1].Value
    $core=$item.Groups[2].Value
    # FAT32 is case-insensitive. Keep the first mapping, like the frontend.
    if($folders.ContainsKey($name)){continue}
    if(Test-Path -LiteralPath (Join-Path $card "cubegm/cores/$core")){
        $folders[$name]=$core
    }
}
foreach($app in @(@('pico286','pico286'),@('lgpt','lgpt'),@('ps1','pcsx4all'))){
    if(Test-Path -LiteralPath (Join-Path $card "cubegm/$($app[1])")){
        $folders[$app[0]]=$app[1]+' (standalone)'
    }
}
$folders['Ebook']='Ebook reader / text editor documents'
$lines=@('# Where to put your games','','All listed folders are included, even when empty. Copy your own games into the matching folder under roms/. No game ROMs or console BIOS files are supplied.','','Common folders: gba = Game Boy Advance; gb = Game Boy; gbc = Game Boy Color; nes = NES; snes = SNES; ps1 = PlayStation; MD = Mega Drive/Genesis; SMS = Master System; gg = Game Gear; arcade = FBNeo arcade.','','Alternative-core folders are optional: normally use the common folder and choose an emulator in Game Details. Platform cards appear when supported games are present. Some imported cores are experimental and may need BIOS/data files; their presence does not guarantee playable performance. See cores.md for system names, requirements and sources.','','| Folder | Default core / app |','| --- | --- |')
foreach($name in ($folders.Keys | Sort-Object)){
    New-Item -ItemType Directory -Force -Path (Join-Path $card "roms/$name") | Out-Null
    $lines+='| `roms/'+$name+'/` | '+$folders[$name]+' |'
}
[IO.File]::WriteAllLines((Join-Path $card 'ROM-FOLDERS.md'),$lines,(New-Object Text.UTF8Encoding($false)))
Write-Host "Created/verified $($folders.Count) ROM/app folders and ROM-FOLDERS.md."
