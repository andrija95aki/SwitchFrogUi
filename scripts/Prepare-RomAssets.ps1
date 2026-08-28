param(
    [Parameter(Mandatory = $true)][string]$CandidateRoot,
    [Parameter(Mandatory = $true)][string]$ExistingCardRoot,
    [Parameter(Mandatory = $true)][string]$DatabaseRoot,
    [Parameter(Mandatory = $true)][string]$ReportPath
)

$ErrorActionPreference = 'Stop'

$systems = @{
    gb = @{
        ThumbnailRepo = 'Nintendo_-_Game_Boy'
        CheatDirectory = 'Nintendo - Game Boy'
        ExistingArt = @('roms\gb\GB\.res', 'roms\gb\.res')
    }
    gbc = @{
        ThumbnailRepo = 'Nintendo_-_Game_Boy_Color'
        CheatDirectory = 'Nintendo - Game Boy Color'
        ExistingArt = @('roms\gb\GBC\.res', 'roms\gbc\.res')
    }
    gba = @{
        ThumbnailRepo = 'Nintendo_-_Game_Boy_Advance'
        CheatDirectory = 'Nintendo - Game Boy Advance'
        ExistingArt = @('roms\gba\.res')
    }
    snes = @{
        ThumbnailRepo = 'Nintendo_-_Super_Nintendo_Entertainment_System'
        CheatDirectory = 'Nintendo - Super Nintendo Entertainment System'
        ExistingArt = @('roms\snes\.res')
    }
}

function Get-NormalizedTitle([string]$Name) {
    $value = [IO.Path]::GetFileNameWithoutExtension($Name)
    $value = $value -replace '\s*\([^)]*\)', ''
    $value = $value -replace '\s*\[[^]]*\]', ''
    $value = $value -replace '(?i)^the\s+', ''
    return ($value -replace '[^a-zA-Z0-9]', '').ToLowerInvariant()
}

function Get-UniqueIndex($Items, [scriptblock]$NameSelector) {
    $groups = @{}
    foreach ($item in $Items) {
        $key = Get-NormalizedTitle (& $NameSelector $item)
        if (-not $key) { continue }
        if (-not $groups.ContainsKey($key)) { $groups[$key] = @() }
        $groups[$key] += $item
    }
    $result = @{}
    foreach ($key in $groups.Keys) {
        if ($groups[$key].Count -eq 1) { $result[$key] = $groups[$key][0] }
    }
    return $result
}

$report = @()
foreach ($platform in @('gb', 'gbc', 'gba', 'snes')) {
    $system = $systems[$platform]
    $romDirectory = Join-Path $CandidateRoot "roms\$platform"
    if (-not (Test-Path -LiteralPath $romDirectory)) { continue }

    $roms = @(Get-ChildItem -LiteralPath $romDirectory -File |
        Where-Object { $_.Name -ne '.metadata.tsv' })
    $artDirectory = Join-Path $romDirectory '.res'
    New-Item -ItemType Directory -Force -Path $artDirectory | Out-Null

    $existingArt = @()
    foreach ($relative in $system.ExistingArt) {
        $directory = Join-Path $ExistingCardRoot $relative
        if (Test-Path -LiteralPath $directory) {
            $existingArt += Get-ChildItem -LiteralPath $directory -File -Filter '*.png'
        }
    }
    $existingExact = @{}
    foreach ($image in $existingArt) { $existingExact[$image.BaseName] = $image }
    $existingFuzzy = Get-UniqueIndex $existingArt { param($item) $item.BaseName }

    $api = "https://api.github.com/repos/libretro-thumbnails/$($system.ThumbnailRepo)/git/trees/master?recursive=1"
    $tree = Invoke-RestMethod -Uri $api -Headers @{ 'User-Agent' = 'SwitchFrogUI-ROM-import' } -TimeoutSec 60
    $remoteArt = @($tree.tree | Where-Object {
        $_.type -eq 'blob' -and $_.path -like 'Named_Boxarts/*.png'
    })
    $remoteExact = @{}
    foreach ($entry in $remoteArt) {
        $remoteExact[[IO.Path]::GetFileNameWithoutExtension($entry.path)] = $entry
    }
    $remoteFuzzy = Get-UniqueIndex $remoteArt { param($item) [IO.Path]::GetFileNameWithoutExtension($item.path) }

    $cheatDirectory = Join-Path $DatabaseRoot "cht\$($system.CheatDirectory)"
    $cheats = if (Test-Path -LiteralPath $cheatDirectory) {
        @(Get-ChildItem -LiteralPath $cheatDirectory -File -Filter '*.cht')
    } else { @() }
    $cheatExact = @{}
    foreach ($cheat in $cheats) { $cheatExact[$cheat.BaseName] = $cheat }
    $cheatFuzzy = Get-UniqueIndex $cheats { param($item) $item.BaseName }
    $cheatOutput = Join-Path $CandidateRoot "picoarch\$platform\cheats"
    New-Item -ItemType Directory -Force -Path $cheatOutput | Out-Null

    foreach ($rom in $roms) {
        $artStatus = 'Missing'
        $artSource = $null
        $destination = Join-Path $artDirectory "$($rom.BaseName).png"
        if (Test-Path -LiteralPath $destination) {
            $bytes = [IO.File]::ReadAllBytes($destination)
            if ($bytes.Length -ge 8 -and $bytes[0] -eq 0x89 -and $bytes[1] -eq 0x50 -and $bytes[2] -eq 0x4E -and $bytes[3] -eq 0x47) {
                $artStatus = 'Already staged'
            }
        }
        if ($artStatus -eq 'Already staged') {
            # Keep an already verified image when a previous run was interrupted.
        } elseif ($existingExact.ContainsKey($rom.BaseName)) {
            $artSource = $existingExact[$rom.BaseName]
            Copy-Item -LiteralPath $artSource.FullName -Destination (Join-Path $artDirectory "$($rom.BaseName).png") -Force
            $artStatus = 'Existing exact'
        } elseif ($remoteExact.ContainsKey($rom.BaseName)) {
            $entry = $remoteExact[$rom.BaseName]
            $encoded = [Uri]::EscapeDataString([IO.Path]::GetFileName($entry.path))
            $url = "https://raw.githubusercontent.com/libretro-thumbnails/$($system.ThumbnailRepo)/master/Named_Boxarts/$encoded"
            try {
                & curl.exe -L --fail --silent --show-error --max-time 60 --output $destination $url
                if ($LASTEXITCODE -ne 0) { throw "curl exited with code $LASTEXITCODE" }
                $bytes = [IO.File]::ReadAllBytes($destination)
                if ($bytes.Length -lt 8 -or $bytes[0] -ne 0x89 -or $bytes[1] -ne 0x50 -or $bytes[2] -ne 0x4E -or $bytes[3] -ne 0x47) {
                    throw 'Downloaded file is not PNG data.'
                }
                $artStatus = 'Downloaded exact'
            } catch {
                if (Test-Path -LiteralPath $destination) { Remove-Item -LiteralPath $destination -Force }
                $artStatus = "Download failed: $($_.Exception.Message)"
            }
        } else {
            $key = Get-NormalizedTitle $rom.BaseName
            if ($existingFuzzy.ContainsKey($key)) {
                $artSource = $existingFuzzy[$key]
                Copy-Item -LiteralPath $artSource.FullName -Destination (Join-Path $artDirectory "$($rom.BaseName).png") -Force
                $artStatus = 'Existing normalized-title match'
            } elseif ($remoteFuzzy.ContainsKey($key)) {
                $entry = $remoteFuzzy[$key]
                $encoded = [Uri]::EscapeDataString([IO.Path]::GetFileName($entry.path))
                $url = "https://raw.githubusercontent.com/libretro-thumbnails/$($system.ThumbnailRepo)/master/Named_Boxarts/$encoded"
                $destination = Join-Path $artDirectory "$($rom.BaseName).png"
                try {
                    & curl.exe -L --fail --silent --show-error --max-time 60 --output $destination $url
                    if ($LASTEXITCODE -ne 0) { throw "curl exited with code $LASTEXITCODE" }
                    $bytes = [IO.File]::ReadAllBytes($destination)
                    if ($bytes.Length -lt 8 -or $bytes[0] -ne 0x89 -or $bytes[1] -ne 0x50 -or $bytes[2] -ne 0x4E -or $bytes[3] -ne 0x47) {
                        throw 'Downloaded file is not PNG data.'
                    }
                    $artStatus = 'Downloaded normalized-title match'
                } catch {
                    if (Test-Path -LiteralPath $destination) { Remove-Item -LiteralPath $destination -Force }
                    $artStatus = "Download failed: $($_.Exception.Message)"
                }
            }
        }

        $cheatStatus = 'Missing'
        $cheat = $null
        if ($cheatExact.ContainsKey($rom.BaseName)) {
            $cheat = $cheatExact[$rom.BaseName]
            $cheatStatus = 'Exact'
        } else {
            $key = Get-NormalizedTitle $rom.BaseName
            if ($cheatFuzzy.ContainsKey($key)) {
                $cheat = $cheatFuzzy[$key]
                $cheatStatus = 'Normalized-title match'
            }
        }
        if ($cheat) {
            Copy-Item -LiteralPath $cheat.FullName -Destination (Join-Path $cheatOutput "$($rom.BaseName).cht") -Force
        }

        $report += [pscustomobject]@{
            Platform = $platform
            Rom = $rom.Name
            Artwork = $artStatus
            Cheat = $cheatStatus
        }
    }
}

$reportDirectory = Split-Path -Parent $ReportPath
if ($reportDirectory) { New-Item -ItemType Directory -Force -Path $reportDirectory | Out-Null }
$report | Export-Csv -LiteralPath $ReportPath -NoTypeInformation -Encoding UTF8
$report | Group-Object Platform | ForEach-Object {
    $rows = @($_.Group)
    [pscustomobject]@{
        Platform = $_.Name
        ROMs = $rows.Count
        Artwork = @($rows | Where-Object { $_.Artwork -notlike 'Missing*' -and $_.Artwork -notlike 'Download failed*' }).Count
        Cheats = @($rows | Where-Object { $_.Cheat -ne 'Missing' }).Count
    }
} | Format-Table -AutoSize
