param(
    [Parameter(Mandatory = $true)][string]$TargetCardRoot,
    [Parameter(Mandatory = $true)][string]$CandidateRoot,
    [Parameter(Mandatory = $true)][string]$BackupRoot,
    [Parameter(Mandatory = $true)][string]$ReportPath
)

$ErrorActionPreference = 'Stop'
$targetRoot = (Resolve-Path -LiteralPath $TargetCardRoot).Path
$candidateRoot = (Resolve-Path -LiteralPath $CandidateRoot).Path
$backupRoot = [IO.Path]::GetFullPath($BackupRoot)
New-Item -ItemType Directory -Force -Path $backupRoot | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
$romExtensions = @('.zip', '.gb', '.gbc', '.gba', '.nes', '.sfc', '.smc')
$zipCrcField = $null

function Get-Fingerprint([IO.FileInfo]$File) {
    if ($File.Extension -ieq '.zip') {
        $archive = [IO.Compression.ZipFile]::OpenRead($File.FullName)
        try {
            $entry = @($archive.Entries | Where-Object { -not [string]::IsNullOrWhiteSpace($_.Name) } |
                Sort-Object Length -Descending)[0]
            if (-not $script:zipCrcField) {
                $script:zipCrcField = $entry.GetType().GetField('_crc32', [Reflection.BindingFlags]'NonPublic,Instance')
            }
            return ('{0}:{1:X8}' -f $entry.Length, [uint32]$script:zipCrcField.GetValue($entry))
        } finally { $archive.Dispose() }
    }
    return ('{0}:{1}' -f $File.Length, (Get-FileHash -LiteralPath $File.FullName -Algorithm SHA256).Hash)
}

function Get-PreferenceScore([IO.FileInfo]$File, $CandidateNames) {
    $name = $File.Name
    $score = 0
    if ($CandidateNames.Contains($name)) { $score += 1000 }
    if ($name -match '(?i)\((USA|World|Europe|USA, Europe|Japan, USA)') { $score += 200 }
    if ($name -match '(?i)\(En([,)])') { $score += 80 }
    if ($name -match '(?i)\(Japan\)' -and $name -notmatch '(?i)\(En([,)])') { $score -= 200 }
    if ($name -match '(?i)Chinese|Japan only|Korea') { $score -= 300 }
    if ($name -match '\[[0-9A-Fa-f]{8}\]') { $score -= 120 }
    if ($name -match '(?i)\(Beta\)|\[b[0-9]*\]|\[t[+0-9]*\]|\[h[0-9]*\]') { $score -= 100 }
    $art = Join-Path $File.DirectoryName ".res\$($File.BaseName).png"
    if (Test-Path -LiteralPath $art) { $score += 10 }
    return $score
}

function Move-Backup([string]$Path, [string]$Reason, [string]$Platform) {
    if (-not (Test-Path -LiteralPath $Path)) { return }
    $full = [IO.Path]::GetFullPath($Path)
    if (-not $full.StartsWith($targetRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to move outside target card: $full"
    }
    $relative = $full.Substring($targetRoot.Length).TrimStart('\')
    $destination = Join-Path $backupRoot $relative
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
    if (Test-Path -LiteralPath $destination) {
        $destination += '.' + (Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash.Substring(0, 12)
    }
    $bytes = (Get-Item -LiteralPath $full).Length
    Move-Item -LiteralPath $full -Destination $destination
    $script:report += [pscustomobject]@{
        Platform = $Platform
        Removed = $relative
        Kept = $script:keptRelative
        Bytes = $bytes
        Reason = $Reason
        Backup = $destination
    }
}

$report = @()
$migrations = [ordered]@{}
foreach ($platform in @('gb', 'gbc', 'gba', 'nes', 'snes')) {
    $directory = Join-Path $targetRoot "roms\$platform"
    if (-not (Test-Path -LiteralPath $directory)) { continue }
    $candidateNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $candidateDirectory = Join-Path $candidateRoot "roms\$platform"
    if (Test-Path -LiteralPath $candidateDirectory) {
        Get-ChildItem -LiteralPath $candidateDirectory -File | ForEach-Object { [void]$candidateNames.Add($_.Name) }
    }
    $files = @(Get-ChildItem -LiteralPath $directory -Recurse -File | Where-Object {
        $_.FullName -notmatch '[\\/]\.res[\\/]' -and $_.Name -ne '.metadata.tsv' -and
        $_.Extension.ToLowerInvariant() -in $romExtensions
    })
    $rows = foreach ($file in $files) {
        [pscustomobject]@{ File = $file; Fingerprint = Get-Fingerprint $file; Score = Get-PreferenceScore $file $candidateNames }
    }
    foreach ($group in @($rows | Group-Object Fingerprint | Where-Object Count -gt 1)) {
        $ordered = @($group.Group | Sort-Object @{ Expression = 'Score'; Descending = $true },
            @{ Expression = { $_.File.Name.Length }; Descending = $false },
            @{ Expression = { $_.File.Name }; Descending = $false })
        $keep = $ordered[0].File
        $script:keptRelative = $keep.FullName.Substring($targetRoot.Length).TrimStart('\') -replace '\\', '/'
        $keepArt = Join-Path $keep.DirectoryName ".res\$($keep.BaseName).png"
        foreach ($row in $ordered | Select-Object -Skip 1) {
            $remove = $row.File
            $oldRelative = $remove.FullName.Substring($targetRoot.Length).TrimStart('\') -replace '\\', '/'
            $migrations["/mnt/sdcard/$oldRelative"] = "/mnt/sdcard/$script:keptRelative"
            $removeArt = Join-Path $remove.DirectoryName ".res\$($remove.BaseName).png"
            if (-not (Test-Path -LiteralPath $keepArt) -and (Test-Path -LiteralPath $removeArt)) {
                New-Item -ItemType Directory -Force -Path (Split-Path -Parent $keepArt) | Out-Null
                Copy-Item -LiteralPath $removeArt -Destination $keepArt
            }
            if (Test-Path -LiteralPath $removeArt) { Move-Backup $removeArt 'Artwork for exact duplicate' $platform }
            Move-Backup $remove.FullName 'Exact ROM payload duplicate' $platform
        }
    }
}

foreach ($relative in @('frogui\favorites.txt', 'frogui\playtime.txt', 'frogui\state_playtime.txt',
        'cubegm\favorites.lst', 'cubegm\recent.lst', 'game_history.txt')) {
    $path = Join-Path $targetRoot $relative
    if (-not (Test-Path -LiteralPath $path)) { continue }
    $recordBackup = Join-Path $backupRoot "user-records\$relative"
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $recordBackup) | Out-Null
    Copy-Item -LiteralPath $path -Destination $recordBackup -Force
    $text = [IO.File]::ReadAllText($path)
    foreach ($old in $migrations.Keys) { $text = $text.Replace($old, $migrations[$old]) }
    [IO.File]::WriteAllText($path, $text, [Text.UTF8Encoding]::new($false))
}

$auxiliary = @(Get-ChildItem -LiteralPath $targetRoot -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -notmatch '[\\/]roms[\\/]' })
foreach ($oldPath in $migrations.Keys) {
    $oldBase = [IO.Path]::GetFileNameWithoutExtension($oldPath)
    $newBase = [IO.Path]::GetFileNameWithoutExtension($migrations[$oldPath])
    $newPlatform = (($migrations[$oldPath] -replace '^/mnt/sdcard/roms/', '') -split '/')[0]
    foreach ($file in $auxiliary | Where-Object {
            $_.Name.StartsWith("$oldBase.", [StringComparison]::OrdinalIgnoreCase) -and
            ($_.FullName -match "(?i)[\\/]picoarch[\\/]$([regex]::Escape($newPlatform))[\\/]" -or
             ($_.FullName -match '(?i)[\\/]cubegm[\\/]saves[\\/]' -and
              $_.Name -match '(?i)\.(sav|srm|state[0-9]*|scr\.bmp)$'))
        }) {
        $suffix = $file.Name.Substring($oldBase.Length)
        $destination = Join-Path $file.DirectoryName ($newBase + $suffix)
        if (-not (Test-Path -LiteralPath $destination)) { Copy-Item -LiteralPath $file.FullName -Destination $destination }
    }
}

$reportDirectory = Split-Path -Parent $ReportPath
if ($reportDirectory) { New-Item -ItemType Directory -Force -Path $reportDirectory | Out-Null }
$report | Export-Csv -LiteralPath $ReportPath -NoTypeInformation -Encoding UTF8
$report | Group-Object Platform | ForEach-Object {
    [pscustomobject]@{ Platform = $_.Name; RemovedFiles = $_.Count; Bytes = ($_.Group | Measure-Object Bytes -Sum).Sum }
} | Format-Table -AutoSize
