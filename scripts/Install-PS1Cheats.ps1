param(
    [Parameter(Mandatory = $true)][string]$RomDirectory,
    [Parameter(Mandatory = $true)][string]$ChtdbRoot,
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [string]$ReportPath
)

$ErrorActionPreference = 'Stop'
$romRoot = (Resolve-Path -LiteralPath $RomDirectory).Path
$database = Join-Path (Resolve-Path -LiteralPath $ChtdbRoot).Path 'cheats'
if (-not (Test-Path -LiteralPath $database -PathType Container)) {
    throw "CHTDB cheats directory not found: $database"
}

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$outputRoot = (Resolve-Path -LiteralPath $OutputDirectory).Path
$serialPattern = [regex]'(?i)(S[CL][A-Z]{2})[_\.-]?([0-9]{3})[_\.-]?([0-9]{2})'
$codePattern = [regex]'^(?<address>[0-9A-Fa-f]{8})\s+(?<value>[0-9A-Fa-f]{4})$'
$codeLikePattern = [regex]'^[0-9A-Fa-f?]{8}\s+[0-9A-Fa-f?]{4}$'
$supportedOpcodes = @(
    0x10,0x11,0x20,0x21,0x1F,0x30,0x50,0x80,
    0xC0,0xC1,0xC2,
    0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,
    0xE0,0xE1,0xE2,0xE3
)

function Get-DiscSerials {
    param([Parameter(Mandatory = $true)][string]$Path)

    # SYSTEM.CNF/PS-X EXE identifiers are near the beginning of raw images and
    # are repeated in PBP metadata.  A bounded read avoids hashing/scanning
    # tens of gigabytes of compressed disc payload solely for an identifier.
    $stream = [IO.File]::OpenRead($Path)
    try {
        $count = [int][Math]::Min([int64](8MB), $stream.Length)
        $bytes = New-Object byte[] $count
        $read = 0
        while ($read -lt $count) {
            $got = $stream.Read($bytes, $read, $count - $read)
            if ($got -le 0) { break }
            $read += $got
        }
        $text = [Text.Encoding]::ASCII.GetString($bytes, 0, $read)
        $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        foreach ($match in $serialPattern.Matches($text)) {
            $canonical = ($match.Groups[1].Value + $match.Groups[2].Value +
                          $match.Groups[3].Value).ToUpperInvariant()
            if ($canonical.Substring(4) -ne '00000') {
                [void]$seen.Add($canonical)
            }
        }
        $serials = @($seen | Sort-Object)
        if ($serials.Count -eq 0 -and [IO.Path]::GetExtension($Path) -ieq '.cue') {
            $cue = [Text.Encoding]::ASCII.GetString($bytes, 0, $read)
            if ($cue -match '(?im)^\s*FILE\s+"([^"]+)"') {
                $disc = Join-Path (Split-Path -Parent $Path) $Matches[1]
                if (Test-Path -LiteralPath $disc -PathType Leaf) {
                    return @(Get-DiscSerials -Path $disc)
                }
            }
        }
        return $serials
    } finally {
        $stream.Dispose()
    }
}

function Convert-ChtdbFile {
    param(
        [Parameter(Mandatory = $true)][string]$InputPath,
        [Parameter(Mandatory = $true)][string]$OutputPath
    )

    $sections = [Collections.Generic.List[object]]::new()
    $current = $null
    foreach ($raw in Get-Content -LiteralPath $InputPath) {
        $line = $raw.Trim()
        if ($line -match '^\[(.+)\]$') {
            if ($null -ne $current) { $sections.Add($current) }
            $current = [ordered]@{
                Name = $Matches[1].Replace('\', ' - ')
                Type = ''
                Codes = [Collections.Generic.List[string]]::new()
                Unsafe = $false
            }
            continue
        }
        if ($null -eq $current -or -not $line -or $line.StartsWith(';')) { continue }
        if ($line -match '^Type\s*=\s*(.+)$') {
            $current.Type = $Matches[1].Trim()
            continue
        }
        $code = $codePattern.Match($line)
        if ($code.Success) {
            $opcode = [Convert]::ToInt32($code.Groups['address'].Value.Substring(0,2), 16)
            if ($supportedOpcodes -notcontains $opcode) {
                $current.Unsafe = $true
            } else {
                $current.Codes.Add(($code.Groups['address'].Value + ' ' +
                                    $code.Groups['value'].Value).ToUpperInvariant())
            }
        } elseif ($codeLikePattern.IsMatch($line)) {
            # Parameter placeholders such as ????/00?? require a user choice
            # which PCSX4ALL's simple toggle menu cannot represent safely.
            $current.Unsafe = $true
        }
    }
    if ($null -ne $current) { $sections.Add($current) }

    $out = [Collections.Generic.List[string]]::new()
    $kept = 0
    foreach ($section in $sections) {
        if ($section.Type -notmatch '^(?i:GameShark)$' -or
            $section.Unsafe -or $section.Codes.Count -eq 0) { continue }
        $out.Add('#' + $section.Name)
        foreach ($code in $section.Codes) { $out.Add($code) }
        $out.Add('')
        $kept++
    }
    if ($kept -eq 0) { return 0 }
    [IO.File]::WriteAllLines($OutputPath, $out, [Text.Encoding]::ASCII)
    return $kept
}

$rows = [Collections.Generic.List[object]]::new()
$installed = @{}
$romFiles = Get-ChildItem -LiteralPath $romRoot -File |
    Where-Object { $_.Extension -match '(?i)^\.(pbp|bin|cue|iso|img|ccd|chd|m3u|toc|cbn|pkg)$' } |
    Sort-Object Name

foreach ($rom in $romFiles) {
    $serials = @(Get-DiscSerials -Path $rom.FullName)
    if ($serials.Count -eq 0) {
        $rows.Add([pscustomobject]@{ Game=$rom.Name; Serial=''; Status='No disc serial found'; Cheats=0 })
        continue
    }
    foreach ($serial in $serials) {
        $databaseName = $serial.Substring(0,4) + '-' + $serial.Substring(4) + '.cht'
        $source = Join-Path $database $databaseName
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
            $rows.Add([pscustomobject]@{ Game=$rom.Name; Serial=$serial; Status='No CHTDB entry'; Cheats=0 })
            continue
        }
        if ($installed.ContainsKey($serial)) {
            $rows.Add([pscustomobject]@{ Game=$rom.Name; Serial=$serial; Status='Shared serial already installed'; Cheats=$installed[$serial] })
            continue
        }
        $destination = Join-Path $outputRoot ($serial + '.txt')
        $count = Convert-ChtdbFile -InputPath $source -OutputPath $destination
        if ($count -gt 0) {
            $installed[$serial] = $count
            $rows.Add([pscustomobject]@{ Game=$rom.Name; Serial=$serial; Status='Installed'; Cheats=$count })
        } else {
            $rows.Add([pscustomobject]@{ Game=$rom.Name; Serial=$serial; Status='No compatible fixed-value codes'; Cheats=0 })
        }
    }
}

$sourceRevision = ''
try { $sourceRevision = (& git -C (Split-Path -Parent $database) rev-parse HEAD 2>$null).Trim() } catch {}
@(
    '# SwitchFrogUI PS1 cheat source',
    '',
    'Generated from DuckStation CHTDB: https://github.com/duckstation/chtdb',
    "Revision: $sourceRevision",
    '',
    'Only fixed-value GameShark sections using opcodes implemented by this',
    'PCSX4ALL build were converted. Cheats start disabled and must be enabled',
    'from the emulator menu. Codes remain the work of their original authors.'
) | Set-Content -LiteralPath (Join-Path $outputRoot 'SOURCE.md') -Encoding utf8

if (-not $ReportPath) { $ReportPath = Join-Path $outputRoot 'INSTALL-REPORT.csv' }
$rows | Export-Csv -LiteralPath $ReportPath -NoTypeInformation -Encoding utf8
$installedGames = @($rows | Where-Object Status -eq 'Installed').Count
$coveredSerials = $installed.Count
Write-Host "PS1 cheat scan complete: $($romFiles.Count) files, $coveredSerials serials installed ($installedGames first-use rows)."
Write-Host "Report: $ReportPath"
