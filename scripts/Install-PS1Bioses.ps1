param(
    [Parameter(Mandatory = $true)][string]$SourceDirectory,
    [Parameter(Mandatory = $true)][string]$CardRoot
)

$ErrorActionPreference = 'Stop'
$sourceRoot = (Resolve-Path -LiteralPath $SourceDirectory).Path
$card = (Resolve-Path -LiteralPath $CardRoot).Path
$biosDirectory = Join-Path $card 'cubegm\bios'
$pcsxDirectory = Join-Path $card 'cubegm\cores\.pcsx4all'
$configDirectory = Join-Path $pcsxDirectory 'config'
$romDirectory = Join-Path $card 'roms\ps1'
$archiveDirectory = Join-Path $card 'PS1 BIOSES'

New-Item -ItemType Directory -Force -Path $biosDirectory,$configDirectory,$archiveDirectory | Out-Null

$known = @{
    # Sony retail BIOS hashes and normalized aliases used by the two PS1 cores.
    'B05DEF971D8EC59F346F2D9AC21FB742E3EB6917' = @{
        Description = 'SCPH-5500 v3.0 NTSC-J'; Aliases = @('scph5500.bin')
    }
    '10155D8D6E6E832D6EA66DB9BC098321FB5E8EBF' = @{
        Description = 'SCPH-1001/5003 v2.2 NTSC-U'; Aliases = @('scph1001.bin')
    }
    '0555C6FAE8906F3F09BAF5988F00E55F88E9F30B' = @{
        Description = 'SCPH-5501/5503/7003 v3.0 NTSC-U'; Aliases = @('scph5501.bin','scph7003.bin')
    }
    '8D5DE56A79954F29E9006929BA3FED9B6A418C1D' = @{
        Description = 'SCPH-7002/7502/9002 v4.1 PAL'; Aliases = @('scph5502.bin','scph7502.bin','scph9002.bin')
    }
}
$badHashes = @{
    'F8DE9325FC36FCFA4B29124D291C9251094F2E54' = 'Known bad SCPH-5502 dump (excluded)'
}

$installed = [Collections.Generic.List[object]]::new()
$rejected = [Collections.Generic.List[object]]::new()
foreach ($file in Get-ChildItem -LiteralPath $sourceRoot -File | Sort-Object Name) {
    $sha1 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA1).Hash.ToUpperInvariant()
    # Preserve every supplied original on card images other than the source
    # card itself, including images deliberately rejected from active use.
    $archivePath = Join-Path $archiveDirectory $file.Name
    if ($file.FullName -ne $archivePath) {
        Copy-Item -LiteralPath $file.FullName -Destination $archivePath -Force
    }
    if ($badHashes.ContainsKey($sha1)) {
        $rejected.Add([pscustomobject]@{ File=$file.Name; SHA1=$sha1; Reason=$badHashes[$sha1] })
        continue
    }
    if (-not $known.ContainsKey($sha1)) {
        $rejected.Add([pscustomobject]@{ File=$file.Name; SHA1=$sha1; Reason='Unrecognized or non-retail BIOS (not installed)' })
        continue
    }
    if ($file.Length -ne 0x80000) {
        $rejected.Add([pscustomobject]@{ File=$file.Name; SHA1=$sha1; Reason='Incorrect size (not installed)' })
        continue
    }

    foreach ($alias in $known[$sha1].Aliases) {
        $destination = Join-Path $biosDirectory $alias
        Copy-Item -LiteralPath $file.FullName -Destination $destination -Force
        $installed.Add([pscustomobject]@{
            Source=$file.Name; Alias=$alias; Description=$known[$sha1].Description; SHA1=$sha1
        })
    }
}

foreach ($required in 'scph5500.bin','scph5501.bin','scph5502.bin') {
    if (-not (Test-Path -LiteralPath (Join-Path $biosDirectory $required) -PathType Leaf)) {
        throw "Required normalized regional BIOS was not installed: $required"
    }
}

function Set-ConfigValue {
    param([string]$Path, [string]$Name, [string]$Value)
    $lines = [Collections.Generic.List[string]]::new()
    if (Test-Path -LiteralPath $Path -PathType Leaf) {
        foreach ($line in Get-Content -LiteralPath $Path) { $lines.Add($line) }
    } else { $lines.Add('CONFIG_VERSION 0') }
    $matched = $false
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match ('^' + [regex]::Escape($Name) + '\s')) {
            $lines[$i] = "$Name $Value"
            $matched = $true
        }
    }
    if (-not $matched) { $lines.Add("$Name $Value") }
    [IO.File]::WriteAllLines($Path, $lines, [Text.Encoding]::ASCII)
}

$globalConfig = Join-Path $pcsxDirectory 'pcsx4all.cfg'
Set-ConfigValue -Path $globalConfig -Name 'HLE' -Value '0'
Set-ConfigValue -Path $globalConfig -Name 'BiosDir' -Value '/mnt/sdcard/cubegm/bios'
Set-ConfigValue -Path $globalConfig -Name 'Bios' -Value 'scph5501.bin'

$regionBios = @{ US='scph5501.bin'; PAL='scph5502.bin'; JP='scph5500.bin' }
$reportPath = Join-Path $pcsxDirectory 'cheats\INSTALL-REPORT.csv'
$serialByGame = @{}
if (Test-Path -LiteralPath $reportPath -PathType Leaf) {
    foreach ($row in Import-Csv -LiteralPath $reportPath) {
        if ($row.Serial -and -not $serialByGame.ContainsKey($row.Game)) {
            $serialByGame[$row.Game] = $row.Serial.ToUpperInvariant()
        }
    }
}

function Get-GameRegion {
    param([IO.FileInfo]$Rom)
    $serial = if ($serialByGame.ContainsKey($Rom.Name)) { $serialByGame[$Rom.Name] } else { '' }
    if ($serial -match '^(SCES|SLES)') { return 'PAL' }
    if ($serial -match '^(SCUS|SLUS)') { return 'US' }
    if ($serial -match '^(SCPS|SLPS|SLPM|SCZS|SIPS)') { return 'JP' }
    if ($Rom.Name -match '(?i)\((Europe|Germany|France|Italy|Spain|Australia)\)') { return 'PAL' }
    if ($Rom.Name -match '(?i)\(Japan\)') { return 'JP' }
    if ($Rom.Name -match '(?i)\(USA\)') { return 'US' }
    return 'US'
}

$games = [Collections.Generic.List[object]]::new()
if (Test-Path -LiteralPath $romDirectory -PathType Container) {
    $extensions = @('.pbp','.bin','.cue','.iso','.img','.ccd','.chd','.m3u','.toc','.cbn','.pkg')
    foreach ($rom in Get-ChildItem -LiteralPath $romDirectory -File | Where-Object {
        $extensions -contains $_.Extension.ToLowerInvariant()
    } | Sort-Object Name) {
        $region = Get-GameRegion -Rom $rom
        $configPath = Join-Path $configDirectory ([IO.Path]::GetFileNameWithoutExtension($rom.Name) + '.cfg')
        Set-ConfigValue -Path $configPath -Name 'HLE' -Value '0'
        Set-ConfigValue -Path $configPath -Name 'BiosDir' -Value '/mnt/sdcard/cubegm/bios'
        Set-ConfigValue -Path $configPath -Name 'Bios' -Value $regionBios[$region]
        $games.Add([pscustomobject]@{
            Game=$rom.Name; Serial=$serialByGame[$rom.Name]; Region=$region; Bios=$regionBios[$region]
        })
    }
}

$report = [Collections.Generic.List[string]]::new()
$report.Add('SwitchFrogUI PS1 BIOS installation report')
$report.Add('')
$report.Add('Installed normalized files:')
foreach ($item in $installed) {
    $report.Add("$($item.Alias) <- $($item.Source) | $($item.Description) | SHA1 $($item.SHA1)")
}
$report.Add('')
$report.Add('Excluded source files:')
foreach ($item in $rejected) {
    $report.Add("$($item.File) | $($item.Reason) | SHA1 $($item.SHA1)")
}
$report.Add('')
$report.Add("Per-game PCSX4ALL profiles: $($games.Count)")
$report.Add("US: $(($games | Where-Object Region -eq 'US').Count) | PAL: $(($games | Where-Object Region -eq 'PAL').Count) | JP: $(($games | Where-Object Region -eq 'JP').Count)")
[IO.File]::WriteAllLines((Join-Path $biosDirectory 'PS1-BIOS-INSTALL.txt'), $report, [Text.Encoding]::UTF8)
$games | Export-Csv -LiteralPath (Join-Path $biosDirectory 'PS1-GAME-BIOS-MAP.csv') -NoTypeInformation -Encoding UTF8

Write-Host "Installed $($installed.Count) normalized BIOS copies into $biosDirectory"
Write-Host "Configured $($games.Count) PS1 files: US $(($games | Where-Object Region -eq 'US').Count), PAL $(($games | Where-Object Region -eq 'PAL').Count), JP $(($games | Where-Object Region -eq 'JP').Count)"
Write-Host "Excluded $($rejected.Count) unrecognized/bad source files; see PS1-BIOS-INSTALL.txt"
