param(
    [Parameter(Mandatory = $true)][string]$RawRoot,
    [Parameter(Mandatory = $true)][string]$OutputCardRoot,
    [Parameter(Mandatory = $true)][string]$DatabaseRoot,
    [Parameter(Mandatory = $true)][string]$ReportPath
)

$ErrorActionPreference = 'Stop'

if (-not ('SwitchFrog.ImportCrc32' -as [type])) {
    Add-Type -TypeDefinition @'
using System.IO;
namespace SwitchFrog {
  public static class ImportCrc32 {
    static readonly uint[] T = Build();
    static uint[] Build() {
      var t = new uint[256];
      for (uint i=0;i<256;i++) {
        uint c=i;
        for(int j=0;j<8;j++) c=(c&1)!=0 ? 0xedb88320u^(c>>1) : c>>1;
        t[i]=c;
      }
      return t;
    }
    public static uint Calculate(string path) {
      using (var s=File.OpenRead(path)) {
        uint c=0xffffffffu; int b;
        while ((b=s.ReadByte()) >= 0) c=T[(c^(byte)b)&255]^(c>>8);
        return c^0xffffffffu;
      }
    }
  }
}
'@
}

$systems = @(
    [pscustomobject]@{ Tag='gb';  Extension='.gb';  Dat='Nintendo - Game Boy.dat';         Platform='Game Boy' },
    [pscustomobject]@{ Tag='gbc'; Extension='.gbc'; Dat='Nintendo - Game Boy Color.dat';   Platform='Game Boy Color' },
    [pscustomobject]@{ Tag='gba'; Extension='.gba'; Dat='Nintendo - Game Boy Advance.dat'; Platform='Game Boy Advance' }
)

function Read-DatGames([string]$Path) {
    $map = @{}
    $raw = Get-Content -Raw -LiteralPath $Path
    foreach ($block in [regex]::Matches($raw,'(?ms)^game \(\s*(.*?)^\)')) {
        $body = $block.Groups[1].Value
        $name = [regex]::Match($body,'(?m)^\s*name "([^"]+)"').Groups[1].Value
        foreach ($rom in [regex]::Matches($body,'(?m)^\s*rom \(.*?size (\d+) crc ([0-9A-Fa-f]{8})')) {
            $map[$rom.Groups[2].Value.ToUpperInvariant() + ':' + $rom.Groups[1].Value] = $name
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

function Test-EnglishName([string]$Name) {
    if ($Name -match '\((En)(,|\+|\))') { return $true }
    if ($Name -match '\((USA|Europe|World|Australia|Canada|UK|New Zealand)(,|\))') { return $true }
    if ($Name -match '\((Japan|China|Taiwan|Korea|Hong Kong|Russia|Brazil|France|Germany|Spain|Italy)(,|\))') { return $false }
    return $false
}

function ConvertTo-SafeStem([string]$Name) {
    $safe = $Name -replace '[:/\\*?"<>|]',' - '
    return ($safe -replace '\s+',' ').Trim().TrimEnd('.')
}

if (-not (Test-Path -LiteralPath $RawRoot -PathType Container)) { throw "Raw root not found: $RawRoot" }
if (-not (Test-Path -LiteralPath $DatabaseRoot -PathType Container)) { throw "Database root not found: $DatabaseRoot" }
New-Item -ItemType Directory -Force -Path $OutputCardRoot | Out-Null

$rows = [Collections.Generic.List[object]]::new()
$selected = @{}
foreach ($system in $systems) {
    $datPath = Join-Path (Join-Path $DatabaseRoot 'no-intro') $system.Dat
    $games = Read-DatGames $datPath
    $genres = Read-DatField (Join-Path (Join-Path $DatabaseRoot 'genre') $system.Dat) 'genre'
    $publishers = Read-DatField (Join-Path (Join-Path $DatabaseRoot 'publisher') $system.Dat) 'publisher'
    $years = Read-DatField (Join-Path (Join-Path $DatabaseRoot 'releaseyear') $system.Dat) 'releaseyear'
    $outDir = Join-Path $OutputCardRoot ('roms\' + $system.Tag)
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null

    $candidates = Get-ChildItem -LiteralPath $RawRoot -Recurse -File |
        Where-Object Extension -IEQ $system.Extension |
        Sort-Object @{Expression={ if ($_.FullName -match '(?i)raw-pokemon') { 0 } else { 1 } }},FullName
    foreach ($file in $candidates) {
        $crc = ([SwitchFrog.ImportCrc32]::Calculate($file.FullName)).ToString('X8')
        $key = $crc + ':' + $file.Length
        $canonical = $games[$key]
        $english = $canonical -and (Test-EnglishName $canonical)
        $status = if (-not $canonical) { 'Rejected - not verified by No-Intro' }
                  elseif (-not $english) { 'Rejected - not English' }
                  elseif ($selected.ContainsKey($key)) { 'Duplicate' }
                  else { 'Selected' }
        $outputName = ''
        $genre = if ($genres[$crc]) { $genres[$crc] } else { 'Unclassified' }
        $publisher = $publishers[$crc]
        $year = $years[$crc]
        if ($status -eq 'Selected') {
            $selected[$key] = $true
            $outputName = (ConvertTo-SafeStem $canonical) + $system.Extension
            $destination = Join-Path $outDir $outputName
            if (Test-Path -LiteralPath $destination) {
                $outputName = (ConvertTo-SafeStem $canonical) + ' [' + $crc + ']' + $system.Extension
                $destination = Join-Path $outDir $outputName
            }
            Copy-Item -LiteralPath $file.FullName -Destination $destination -Force
        }
        $facts = @()
        if ($year) { $facts += "released in $year" }
        if ($publisher) { $facts += "published by $publisher" }
        $description = if ($genre -eq 'Unclassified') {
            "An English-language game for $($system.Platform). Exact database metadata was not available."
        } else {
            $text = "A $genre game for $($system.Platform)"
            if ($facts.Count) { $text += ', ' + ($facts -join ' and ') }
            $text + '.'
        }
        $rows.Add([pscustomobject]@{
            Platform=$system.Tag; Source=$file.FullName; CRC=$crc; Size=$file.Length;
            CanonicalName=$canonical; OutputName=$outputName; Status=$status;
            Category=$genre; Description=$description
        })
    }

    $metadata = @('# filename<TAB>category<TAB>description')
    $metadata += @($rows | Where-Object { $_.Platform -eq $system.Tag -and $_.Status -eq 'Selected' } |
        Sort-Object OutputName | ForEach-Object {
            $_.OutputName + "`t" + $_.Category + "`t" + ($_.Description -replace "[`r`n`t]",' ')
        })
    [IO.File]::WriteAllLines((Join-Path $outDir '.metadata.tsv'),[string[]]$metadata,
        (New-Object Text.UTF8Encoding($false)))
}

$reportParent = Split-Path -Parent $ReportPath
if ($reportParent) { New-Item -ItemType Directory -Force -Path $reportParent | Out-Null }
$rows | Export-Csv -LiteralPath $ReportPath -NoTypeInformation -Encoding UTF8
$rows | Group-Object Platform,Status | Sort-Object Name |
    ForEach-Object { [pscustomobject]@{ Group=$_.Name; Count=$_.Count } } |
    Format-Table -AutoSize
Write-Output "Report: $ReportPath"
