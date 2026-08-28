param(
    [Parameter(Mandatory = $true)][string]$CandidateRoot,
    [Parameter(Mandatory = $true)][string]$TargetCardRoot,
    [Parameter(Mandatory = $true)][string]$BackupRoot,
    [Parameter(Mandatory = $true)][string]$ReportPath
)

$ErrorActionPreference = 'Stop'
$candidateRootPath = (Resolve-Path -LiteralPath $CandidateRoot).Path
$targetRootPath = (Resolve-Path -LiteralPath $TargetCardRoot).Path
$backupRootPath = [IO.Path]::GetFullPath($BackupRoot)
New-Item -ItemType Directory -Force -Path $backupRootPath | Out-Null

Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -TypeDefinition @'
using System;
using System.IO;
public static class SwitchFrogCrc32 {
    private static readonly uint[] Table = MakeTable();
    private static uint[] MakeTable() {
        var table = new uint[256];
        for (uint i = 0; i < table.Length; i++) {
            uint value = i;
            for (int bit = 0; bit < 8; bit++)
                value = (value & 1) != 0 ? 0xEDB88320U ^ (value >> 1) : value >> 1;
            table[i] = value;
        }
        return table;
    }
    public static uint File(string path) {
        uint crc = 0xFFFFFFFFU;
        byte[] buffer = new byte[1024 * 1024];
        using (var stream = System.IO.File.OpenRead(path)) {
            int read;
            while ((read = stream.Read(buffer, 0, buffer.Length)) > 0)
                for (int i = 0; i < read; i++) crc = Table[(crc ^ buffer[i]) & 0xFF] ^ (crc >> 8);
        }
        return crc ^ 0xFFFFFFFFU;
    }
}
'@

$romExtensions = @('.gb', '.gbc', '.gba', '.sfc', '.smc', '.zip')
$zipCrcField = $null

function Get-RomFingerprint([IO.FileInfo]$File) {
    if ($File.Extension -ieq '.zip') {
        $archive = [IO.Compression.ZipFile]::OpenRead($File.FullName)
        try {
            $entry = @($archive.Entries | Where-Object {
                -not [string]::IsNullOrWhiteSpace($_.Name) -and
                ([IO.Path]::GetExtension($_.Name).ToLowerInvariant() -in $romExtensions)
            } | Sort-Object Length -Descending)[0]
            if (-not $entry) { return $null }
            if (-not $script:zipCrcField) {
                $script:zipCrcField = $entry.GetType().GetField('_crc32', [Reflection.BindingFlags]'NonPublic,Instance')
            }
            $crc = [uint32]$script:zipCrcField.GetValue($entry)
            return ('{0}:{1:X8}' -f $entry.Length, $crc)
        } finally {
            $archive.Dispose()
        }
    }
    $crc = [SwitchFrogCrc32]::File($File.FullName)
    return ('{0}:{1:X8}' -f $File.Length, $crc)
}

function Move-ToBackup([string]$Path, [string]$Reason) {
    if (-not (Test-Path -LiteralPath $Path)) { return }
    $full = [IO.Path]::GetFullPath($Path)
    if (-not $full.StartsWith($targetRootPath, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to move a file outside the target card: $full"
    }
    $relative = $full.Substring($targetRootPath.Length).TrimStart('\')
    $destination = Join-Path $backupRootPath $relative
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
    if (Test-Path -LiteralPath $destination) {
        $suffix = (Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash.Substring(0, 12)
        $destination = "$destination.$suffix"
    }
    $length = (Get-Item -LiteralPath $full).Length
    Move-Item -LiteralPath $full -Destination $destination
    $script:report += [pscustomobject]@{
        Action = 'Backed up duplicate'
        Platform = $script:currentPlatform
        Source = $relative
        Destination = $destination
        Bytes = $length
        Detail = $Reason
    }
}

function Merge-Metadata([string]$Source, [string]$Destination) {
    $rows = [ordered]@{}
    foreach ($path in @($Destination, $Source)) {
        if (-not (Test-Path -LiteralPath $path)) { continue }
        foreach ($line in Get-Content -LiteralPath $path) {
            if (-not $line -or $line.StartsWith('#')) { continue }
            $parts = $line -split "`t", 3
            if ($parts.Count -ge 3) { $rows[$parts[0]] = $line }
        }
    }
    $lines = @('# filename<TAB>category<TAB>description') + @($rows.Values | Sort-Object)
    [IO.File]::WriteAllLines($Destination, $lines, [Text.UTF8Encoding]::new($false))
}

$report = @()
$pathMigrations = [ordered]@{}
foreach ($platform in @('gb', 'gbc', 'gba', 'snes')) {
    $script:currentPlatform = $platform
    $candidateDirectory = Join-Path $candidateRootPath "roms\$platform"
    if (-not (Test-Path -LiteralPath $candidateDirectory)) { continue }
    $candidates = @(Get-ChildItem -LiteralPath $candidateDirectory -File |
        Where-Object { $_.Name -ne '.metadata.tsv' -and $_.Extension.ToLowerInvariant() -in $romExtensions })
    if ($candidates.Count -eq 0) { continue }

    $destinationDirectory = Join-Path $targetRootPath "roms\$platform"
    New-Item -ItemType Directory -Force -Path $destinationDirectory | Out-Null
    $fingerprints = @{}
    foreach ($candidate in $candidates) {
        $fingerprint = Get-RomFingerprint $candidate
        if ($fingerprints.ContainsKey($fingerprint)) {
            throw "Candidate set contains duplicate ROM payloads: $($candidate.Name) and $($fingerprints[$fingerprint].Name)"
        }
        $fingerprints[$fingerprint] = $candidate
    }

    $scanDirectories = @($destinationDirectory)
    if ($platform -eq 'gbc') {
        $legacy = Join-Path $targetRootPath 'roms\gb\GBC'
        if (Test-Path -LiteralPath $legacy) { $scanDirectories += $legacy }
    }
    $existing = @($scanDirectories | ForEach-Object {
        Get-ChildItem -LiteralPath $_ -Recurse -File | Where-Object {
            $_.FullName -notmatch '[\\/]\.res[\\/]' -and
            $_.Name -ne '.metadata.tsv' -and
            $_.Extension.ToLowerInvariant() -in $romExtensions
        }
    })
    foreach ($old in $existing) {
        $fingerprint = Get-RomFingerprint $old
        if (-not $fingerprints.ContainsKey($fingerprint)) { continue }
        $candidate = $fingerprints[$fingerprint]
        $newPath = Join-Path $destinationDirectory $candidate.Name
        if ([IO.Path]::GetFullPath($old.FullName) -ieq [IO.Path]::GetFullPath($newPath)) { continue }
        $oldRelative = $old.FullName.Substring($targetRootPath.Length).TrimStart('\') -replace '\\', '/'
        $newRelative = $newPath.Substring($targetRootPath.Length).TrimStart('\') -replace '\\', '/'
        $pathMigrations["/mnt/sdcard/$oldRelative"] = "/mnt/sdcard/$newRelative"
        $oldArt = Join-Path $old.DirectoryName ".res\$($old.BaseName).png"
        if (Test-Path -LiteralPath $oldArt) { Move-ToBackup $oldArt 'Artwork for superseded ROM filename' }
        Move-ToBackup $old.FullName "Superseded by newly supplied English ROM $($candidate.Name)"
    }

    foreach ($candidate in $candidates) {
        $destination = Join-Path $destinationDirectory $candidate.Name
        Copy-Item -LiteralPath $candidate.FullName -Destination $destination -Force
        $report += [pscustomobject]@{
            Action = 'Installed ROM'
            Platform = $platform
            Source = $candidate.FullName
            Destination = $destination
            Bytes = $candidate.Length
            Detail = 'New English-only curated set has priority'
        }
    }
    Merge-Metadata (Join-Path $candidateDirectory '.metadata.tsv') (Join-Path $destinationDirectory '.metadata.tsv')

    $candidateArt = Join-Path $candidateDirectory '.res'
    if (Test-Path -LiteralPath $candidateArt) {
        $destinationArt = Join-Path $destinationDirectory '.res'
        New-Item -ItemType Directory -Force -Path $destinationArt | Out-Null
        Get-ChildItem -LiteralPath $candidateArt -File -Filter '*.png' | ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $destinationArt $_.Name) -Force
        }
    }
    $candidateCheats = Join-Path $candidateRootPath "picoarch\$platform\cheats"
    if (Test-Path -LiteralPath $candidateCheats) {
        $destinationCheats = Join-Path $targetRootPath "picoarch\$platform\cheats"
        New-Item -ItemType Directory -Force -Path $destinationCheats | Out-Null
        Get-ChildItem -LiteralPath $candidateCheats -File -Filter '*.cht' | ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $destinationCheats $_.Name) -Force
        }
    }
}

# Keep favourites, recent selections and timing records attached to renamed ROMs.
if ($pathMigrations.Count -gt 0) {
    foreach ($relative in @('frogui\favorites.txt', 'frogui\playtime.txt', 'frogui\state_playtime.txt',
            'cubegm\favorites.lst', 'cubegm\recent.lst', 'game_history.txt')) {
        $path = Join-Path $targetRootPath $relative
        if (-not (Test-Path -LiteralPath $path)) { continue }
        $text = [IO.File]::ReadAllText($path)
        foreach ($oldPath in $pathMigrations.Keys) { $text = $text.Replace($oldPath, $pathMigrations[$oldPath]) }
        [IO.File]::WriteAllText($path, $text, [Text.UTF8Encoding]::new($false))
    }
    # Save RAM, save states, per-game configuration and captures are keyed by
    # ROM basename. Keep the original and add a canonical-name copy when a ROM
    # was superseded, so user data remains usable and rollback stays possible.
    $auxiliary = @(Get-ChildItem -LiteralPath $targetRootPath -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -notmatch '[\\/]roms[\\/]' })
    foreach ($oldPath in $pathMigrations.Keys) {
        $oldBase = [IO.Path]::GetFileNameWithoutExtension($oldPath)
        $newBase = [IO.Path]::GetFileNameWithoutExtension($pathMigrations[$oldPath])
        foreach ($file in $auxiliary | Where-Object { $_.Name.StartsWith("$oldBase.", [StringComparison]::OrdinalIgnoreCase) }) {
            $suffix = $file.Name.Substring($oldBase.Length)
            $destination = Join-Path $file.DirectoryName ($newBase + $suffix)
            if (-not (Test-Path -LiteralPath $destination)) { Copy-Item -LiteralPath $file.FullName -Destination $destination }
        }
    }
}

$reportDirectory = Split-Path -Parent $ReportPath
if ($reportDirectory) { New-Item -ItemType Directory -Force -Path $reportDirectory | Out-Null }
$report | Export-Csv -LiteralPath $ReportPath -NoTypeInformation -Encoding UTF8
$report | Group-Object Action, Platform | ForEach-Object {
    [pscustomobject]@{
        Action = $_.Group[0].Action
        Platform = $_.Group[0].Platform
        Files = $_.Count
        Bytes = ($_.Group | Measure-Object Bytes -Sum).Sum
    }
} | Sort-Object Platform, Action | Format-Table -AutoSize
