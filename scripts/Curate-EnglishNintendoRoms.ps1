param(
    [Parameter(Mandatory = $true)][string]$CardRoot,
    [Parameter(Mandatory = $true)][string]$DatabaseRoot,
    [string]$ReportDirectory = (Join-Path $PSScriptRoot '..\reports'),
    [switch]$Apply
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

if (-not ('SwitchFrog.Crc32' -as [type])) {
    Add-Type -TypeDefinition @'
using System.IO;
namespace SwitchFrog {
  public static class Crc32 {
    static readonly uint[] T = Build();
    static uint[] Build() {
      var t = new uint[256];
      for (uint i=0;i<256;i++) { uint c=i; for(int j=0;j<8;j++) c=(c&1)!=0 ? 0xedb88320u^(c>>1) : c>>1; t[i]=c; }
      return t;
    }
    public static uint Calculate(Stream s, int skip) {
      while (skip-- > 0 && s.ReadByte() >= 0) {}
      uint c=0xffffffffu; int b;
      while ((b=s.ReadByte()) >= 0) c=T[(c^(byte)b)&255]^(c>>8);
      return c^0xffffffffu;
    }
  }
}
'@
}

$crcField = [IO.Compression.ZipArchiveEntry].GetField(
    '_crc32',[Reflection.BindingFlags]'Instance,NonPublic')
if (-not $crcField) { throw 'This PowerShell/.NET build does not expose ZIP CRC metadata.' }

$systems = @(
    [pscustomobject]@{ Tag='gba'; Dat='Nintendo - Game Boy Advance.dat'; Platform='Game Boy Advance' },
    [pscustomobject]@{ Tag='nes'; Dat='Nintendo - Nintendo Entertainment System.dat'; Platform='Nintendo Entertainment System' },
    [pscustomobject]@{ Tag='snes'; Dat='Nintendo - Super Nintendo Entertainment System.dat'; Platform='Super Nintendo' }
)

function Read-DatGames([string]$Path) {
    $map = @{}
    $raw = Get-Content -Raw -LiteralPath $Path
    foreach ($block in [regex]::Matches($raw,'(?ms)^game \(\s*(.*?)^\)')) {
        $body = $block.Groups[1].Value
        $name = [regex]::Match($body,'(?m)^\s*name "([^"]+)"').Groups[1].Value
        foreach ($rom in [regex]::Matches($body,'(?m)^\s*rom \(.*?size (\d+) crc ([0-9A-Fa-f]{8})')) {
            $key = $rom.Groups[2].Value.ToUpperInvariant() + ':' + $rom.Groups[1].Value
            $map[$key] = $name
        }
    }
    return $map
}

function Read-DatField([string]$Path,[string]$Field) {
    $map = @{}
    if (-not (Test-Path -LiteralPath $Path)) { return $map }
    $raw = Get-Content -Raw -LiteralPath $Path
    foreach ($block in [regex]::Matches($raw,'(?ms)^game \(\s*(.*?)^\)')) {
        $body = $block.Groups[1].Value
        $value = [regex]::Match($body,"(?m)^\s*$Field `"([^`"]+)`"").Groups[1].Value
        if (-not $value) { continue }
        foreach ($rom in [regex]::Matches($body,'(?m)^\s*rom \(.*?crc ([0-9A-Fa-f]{8})')) {
            $map[$rom.Groups[1].Value.ToUpperInvariant()] = $value
        }
    }
    return $map
}

function ConvertTo-SafeStem([string]$Name) {
    $safe = $Name -replace '[:/\\*?"<>|]',' - '
    $safe = $safe -replace '\s+',' '
    return $safe.Trim().TrimEnd('.')
}

function Test-EnglishName([string]$Name) {
    if ($Name -match '\((En)(,|\+|\))') { return $true }
    if ($Name -match '\((USA|Europe|World|Australia|Canada|UK|New Zealand)(,|\))') { return $true }
    if ($Name -match '\((Japan|China|Taiwan|Korea|Hong Kong|Russia|Brazil|France|Germany|Spain|Italy)(,|\))') { return $false }
    return $null
}

function Get-ZipIdentity([IO.FileInfo]$File,[string]$Tag) {
    $zip = $null
    try {
        $zip = [IO.Compression.ZipFile]::OpenRead($File.FullName)
        $entry = $zip.Entries | Where-Object { $_.Length -gt 0 } |
            Sort-Object Length -Descending | Select-Object -First 1
        if (-not $entry) { throw 'empty archive' }
        $crc = ([uint32]$crcField.GetValue($entry)).ToString('X8')
        $result = [ordered]@{ Crc=$crc; Size=[int64]$entry.Length; HeaderlessCrc=''; HeaderlessSize=0; GbaRegion='' }
        if ($Tag -eq 'nes' -and $entry.Length -gt 16) {
            $stream = $entry.Open()
            try { $result.HeaderlessCrc = ([SwitchFrog.Crc32]::Calculate($stream,16)).ToString('X8') }
            finally { $stream.Dispose() }
            $result.HeaderlessSize = $entry.Length - 16
        } elseif ($Tag -eq 'snes' -and (($entry.Length % 1024) -eq 512)) {
            $stream = $entry.Open()
            try { $result.HeaderlessCrc = ([SwitchFrog.Crc32]::Calculate($stream,512)).ToString('X8') }
            finally { $stream.Dispose() }
            $result.HeaderlessSize = $entry.Length - 512
        } elseif ($Tag -eq 'gba' -and $entry.Length -gt 176) {
            $stream = $entry.Open()
            try {
                $header = New-Object byte[] 176
                if ($stream.Read($header,0,$header.Length) -eq $header.Length) {
                    $result.GbaRegion = [char]$header[175]
                }
            } finally { $stream.Dispose() }
        }
        return [pscustomobject]$result
    } finally { if ($zip) { $zip.Dispose() } }
}

function Get-FallbackLanguage([string]$Name,[string]$Tag,[string]$GbaRegion) {
    if ($Name -match '(?i)(\(|\[)(JP|JPN|Japan|CN|CHN|China|TW|Taiwan|KR|KOR|Korea)(\)|\])') { return 'NonEnglish' }
    if ($Name -match '(?i)(\(|\[)(US|USA|UE|EU|EUR|Europe|World|EN)(\)|\])') { return 'English' }
    if ($Tag -eq 'gba') {
        if ($GbaRegion -match '[EPX]') { return 'English' }
        if ($GbaRegion -match '[JCK]') { return 'NonEnglish' }
    }
    return 'Review'
}

function Move-OrRenameCompanion([string]$Path,[string]$OldStem,[string]$NewStem,[string]$Quarantine) {
    if (-not (Test-Path -LiteralPath $Path)) { return }
    $item = Get-Item -LiteralPath $Path
    if ($Quarantine) {
        New-Item -ItemType Directory -Force -Path $Quarantine | Out-Null
        Move-Item -LiteralPath $item.FullName -Destination (Join-Path $Quarantine $item.Name)
    } elseif ($OldStem -ne $NewStem) {
        $dest = Join-Path $item.DirectoryName ($NewStem + $item.Extension)
        if (-not (Test-Path -LiteralPath $dest)) { Move-Item -LiteralPath $item.FullName -Destination $dest }
    }
}

if (-not (Test-Path -LiteralPath $CardRoot)) { throw "Card root not found: $CardRoot" }
if (-not (Test-Path -LiteralPath $DatabaseRoot)) { throw "Database root not found: $DatabaseRoot" }
New-Item -ItemType Directory -Force -Path $ReportDirectory | Out-Null
$rows = [Collections.Generic.List[object]]::new()

foreach ($system in $systems) {
    $romDir = Join-Path $CardRoot ("roms\" + $system.Tag)
    if (-not (Test-Path -LiteralPath $romDir)) { continue }
    $games = Read-DatGames (Join-Path (Join-Path $DatabaseRoot 'no-intro') $system.Dat)
    $genres = Read-DatField (Join-Path (Join-Path $DatabaseRoot 'genre') $system.Dat) 'genre'
    $publishers = Read-DatField (Join-Path (Join-Path $DatabaseRoot 'publisher') $system.Dat) 'publisher'
    $years = Read-DatField (Join-Path (Join-Path $DatabaseRoot 'releaseyear') $system.Dat) 'releaseyear'
    foreach ($file in Get-ChildItem -LiteralPath $romDir -Filter *.zip -File) {
        try {
            $id = Get-ZipIdentity $file $system.Tag
            $key = $id.Crc + ':' + $id.Size
            $canonical = $games[$key]
            $metaCrc = $id.Crc
            if (-not $canonical -and $id.HeaderlessCrc) {
                $key = $id.HeaderlessCrc + ':' + $id.HeaderlessSize
                $canonical = $games[$key]
                if ($canonical) { $metaCrc = $id.HeaderlessCrc }
            }
            $language = if ($canonical) { Test-EnglishName $canonical } else { $null }
            $status = if ($language -eq $true) { 'English' } elseif ($language -eq $false) { 'NonEnglish' } else {
                Get-FallbackLanguage $file.BaseName $system.Tag $id.GbaRegion
            }
            $newStem = if ($canonical -and $status -eq 'English') { ConvertTo-SafeStem $canonical } else { $file.BaseName }
            $genre = if ($genres[$metaCrc]) { $genres[$metaCrc] } else { 'Unclassified' }
            $publisher = $publishers[$metaCrc]
            $year = $years[$metaCrc]
            $facts = @()
            if ($year) { $facts += "released in $year" }
            if ($publisher) { $facts += "published by $publisher" }
            if ($genre -eq 'Unclassified') {
                $description = "A game for $($system.Platform). Exact database metadata was not available."
            } else {
                $description = "A $genre game for $($system.Platform)"
                if ($facts.Count) { $description += ', ' + ($facts -join ' and ') }
                $description += '.'
            }
            $rows.Add([pscustomobject]@{
                Platform=$system.Tag; OldName=$file.Name; NewName=($newStem + '.zip');
                Status=$status; Verified=[bool]$canonical; CRC=$metaCrc;
                Category=$genre; Description=$description; Error=''
            })
        } catch {
            $rows.Add([pscustomobject]@{ Platform=$system.Tag; OldName=$file.Name; NewName=$file.Name;
                Status='Review'; Verified=$false; CRC=''; Category='Unknown'; Description=''; Error=$_.Exception.Message })
        }
    }
}

# Some source sets contain multiple valid dumps which No-Intro assigns the
# same display name (usually alternate headers). Keep them all recoverably and
# add only a checksum suffix to the second and later filenames.
foreach ($group in $rows | Where-Object Status -eq 'English' | Group-Object Platform,NewName | Where-Object Count -gt 1) {
    $ordered = @($group.Group | Sort-Object @{Expression={ $_.OldName -ne $_.NewName }},OldName)
    for ($i=1; $i -lt $ordered.Count; $i++) {
        $stem = [IO.Path]::GetFileNameWithoutExtension($ordered[$i].NewName)
        $ordered[$i].NewName = "$stem [$($ordered[$i].CRC)].zip"
    }
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$report = Join-Path $ReportDirectory ("rom-curation-$stamp.csv")
$rows | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath $report

if ($Apply) {
    foreach ($row in $rows) {
        $romDir = Join-Path $CardRoot ("roms\" + $row.Platform)
        $source = Join-Path $romDir $row.OldName
        if (-not (Test-Path -LiteralPath $source)) { continue }
        $oldStem = [IO.Path]::GetFileNameWithoutExtension($row.OldName)
        $newStem = [IO.Path]::GetFileNameWithoutExtension($row.NewName)
        if ($row.Status -eq 'NonEnglish') {
            $q = Join-Path $CardRoot ("Quarantine\Non-English ROMs\" + $row.Platform)
            New-Item -ItemType Directory -Force -Path $q | Out-Null
            Move-Item -LiteralPath $source -Destination (Join-Path $q $row.OldName)
            foreach ($artDir in @('.res','images')) {
                Move-OrRenameCompanion (Join-Path (Join-Path $romDir $artDir) ($oldStem + '.png')) $oldStem $newStem (Join-Path $q $artDir)
            }
            $stateDir = Join-Path $CardRoot ("picoarch\" + $row.Platform)
            if (Test-Path -LiteralPath $stateDir) {
                foreach ($companion in Get-ChildItem -LiteralPath $stateDir -File | Where-Object Name -Like "$oldStem.*") {
                    Move-OrRenameCompanion $companion.FullName $oldStem $newStem (Join-Path $q 'picoarch')
                }
            }
        } elseif ($row.Status -eq 'English' -and $row.OldName -ne $row.NewName) {
            $dest = Join-Path $romDir $row.NewName
            if (-not (Test-Path -LiteralPath $dest)) {
                Move-Item -LiteralPath $source -Destination $dest
                foreach ($artDir in @('.res','images')) {
                    Move-OrRenameCompanion (Join-Path (Join-Path $romDir $artDir) ($oldStem + '.png')) $oldStem $newStem ''
                }
                $stateDir = Join-Path $CardRoot ("picoarch\" + $row.Platform)
                if (Test-Path -LiteralPath $stateDir) {
                    foreach ($companion in Get-ChildItem -LiteralPath $stateDir -File | Where-Object Name -Like "$oldStem.*") {
                        $suffix = $companion.Name.Substring($oldStem.Length)
                        $stateDest = Join-Path $stateDir ($newStem + $suffix)
                        if (-not (Test-Path -LiteralPath $stateDest)) { Move-Item -LiteralPath $companion.FullName -Destination $stateDest }
                    }
                }
            }
        }
    }

    foreach ($system in $systems) {
        $metadataPath = Join-Path (Join-Path $CardRoot ("roms\" + $system.Tag)) '.metadata.tsv'
        $metadataRows = $rows | Where-Object { $_.Platform -eq $system.Tag -and $_.Status -eq 'English' }
        $metadataContent = @('# filename<TAB>category<TAB>description') + @($metadataRows | ForEach-Object {
            $_.NewName + "`t" + $_.Category + "`t" + ($_.Description -replace "[`r`n`t]",' ')
        })
        [IO.File]::WriteAllLines($metadataPath,[string[]]$metadataContent,(New-Object Text.UTF8Encoding($false)))
    }

    # Preserve favourites, play-time and recent paths after verified renames;
    # remove only entries which point to quarantined ROMs.
    foreach ($cfg in Get-ChildItem -LiteralPath (Join-Path $CardRoot 'frogui') -File -ErrorAction SilentlyContinue |
             Where-Object Extension -In '.txt','.cfg') {
        $content = @(Get-Content -LiteralPath $cfg.FullName)
        foreach ($row in $rows) {
            $oldPath = "/mnt/sdcard/roms/$($row.Platform)/$($row.OldName)"
            if ($row.Status -eq 'NonEnglish') { $content = @($content | Where-Object { $_ -notlike "*$oldPath*" }) }
            elseif ($row.OldName -ne $row.NewName) {
                $newPath = "/mnt/sdcard/roms/$($row.Platform)/$($row.NewName)"
                $oldStem = [IO.Path]::GetFileNameWithoutExtension($row.OldName)
                $newStem = [IO.Path]::GetFileNameWithoutExtension($row.NewName)
                $content = @($content | ForEach-Object {
                    if ($_ -like "*$oldPath*") { $_.Replace("|$oldStem|","|$newStem|").Replace($oldPath,$newPath) }
                    else { $_ }
                })
            }
        }
        [IO.File]::WriteAllLines($cfg.FullName,[string[]]$content,(New-Object Text.UTF8Encoding($false)))
    }
}

$summary = $rows | Group-Object Platform,Status | Sort-Object Name |
    ForEach-Object { [pscustomobject]@{ Group=$_.Name; Count=$_.Count } }
$summary | Format-Table -AutoSize
Write-Output "Report: $report"
if (-not $Apply) { Write-Output 'Dry run only. Re-run with -Apply after reviewing the CSV.' }
