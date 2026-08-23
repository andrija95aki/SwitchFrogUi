param(
    [Parameter(Mandatory = $true)][string]$TestedCardRoot,
    [string]$OutputDirectory = (Join-Path (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)) 'dist')
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$card = (Resolve-Path -LiteralPath $TestedCardRoot).Path.TrimEnd('\')
$outParent = [IO.Path]::GetFullPath($OutputDirectory)
$stage = Join-Path $outParent 'SwitchFrogUI-R36SX-HOS-1.2'
$sdRoot = Join-Path $stage 'SD_ROOT'
$zipPath = Join-Path $outParent 'SwitchFrogUI-R36SX-HOS-1.2.zip'

if (-not (Test-Path -LiteralPath (Join-Path $card 'cubegm\zhijack.sh'))) {
    throw 'The selected card tree is not an assembled SwitchFrogUI R36SX card.'
}

New-Item -ItemType Directory -Force -Path $outParent | Out-Null
if (Test-Path -LiteralPath $stage) {
    $resolvedStage = (Resolve-Path -LiteralPath $stage).Path
    if (-not $resolvedStage.StartsWith($outParent, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove stage outside output directory: $resolvedStage"
    }
    Remove-Item -Recurse -Force -LiteralPath $resolvedStage
}
if (Test-Path -LiteralPath $zipPath) { Remove-Item -Force -LiteralPath $zipPath }
New-Item -ItemType Directory -Force -Path $sdRoot | Out-Null

function Copy-CardFile {
    param([Parameter(Mandatory = $true)][string]$RelativePath)
    $source = Join-Path $card $RelativePath
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Missing tested-card file: $RelativePath"
    }
    $destination = Join-Path $sdRoot $RelativePath
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
    Copy-Item -Force -LiteralPath $source -Destination $destination
}

function Copy-CardDirectory {
    param([Parameter(Mandatory = $true)][string]$RelativePath)
    $source = Join-Path $card $RelativePath
    if (-not (Test-Path -LiteralPath $source -PathType Container)) {
        throw "Missing tested-card directory: $RelativePath"
    }
    $destination = Join-Path $sdRoot $RelativePath
    New-Item -ItemType Directory -Force -Path $destination | Out-Null
    Copy-Item -Recurse -Force -Path (Join-Path $source '*') -Destination $destination
}

$runtimeFiles = @(
    'cubegm\picoarch',
    'cubegm\picoarch_hi',
    'cubegm\pcsx4all',
    'cubegm\rockbox',
    'cubegm\rockbox.sh',
    'cubegm\video_player',
    'cubegm\video_player_impl.so',
    'cubegm\video_player.sh',
    'cubegm\nosleep',
    'cubegm\r36sx_displayfix.so',
    'cubegm\driver_r36sx.so',
    'cubegm\driver_r36sx27.so',
    'cubegm\zhijack.sh',
    'cubegm\setting.xml',
    'cubegm\xgame-logo.bmp',
    'cubegm\cores\frogui_libretro.so',
    'cubegm\cores\libemu_md.so',
    'picoarch.cfg'
)
foreach ($file in $runtimeFiles) { Copy-CardFile $file }

# Open-source libretro cores only. Stock libemu_* cores and rollback binaries
# are intentionally excluded; libemu_md.so above is our source-built boot hook.
$coreSource = Join-Path $card 'cubegm\cores'
Get-ChildItem -LiteralPath $coreSource -File -Filter '*_libretro.so' |
    Where-Object { $_.Name -notmatch '\.pre-|\.so\.pre-' } |
    ForEach-Object { Copy-CardFile (Join-Path 'cubegm\cores' $_.Name) }

# Runtime libraries distributed by upstream TreeFrogUI. BIOS payloads in the
# same card folder are excluded by this explicit allow-list.
foreach ($name in @('libSDL-1.2.so.0','libpng12.so.0','libpng16.so.16','libz.so.1')) {
    Copy-CardFile (Join-Path 'cubegm\lib' $name)
}

# Static frontend art only. Settings/history/save metadata is recreated cleanly.
$frogSource = Join-Path $card 'frogui'
New-Item -ItemType Directory -Force -Path (Join-Path $sdRoot 'frogui') | Out-Null
Get-ChildItem -LiteralPath $frogSource -File |
    Where-Object { $_.Extension -match '^\.(jpg|jpeg|png|bmp)$' -or $_.Name -eq 'keymap.txt' } |
    ForEach-Object { Copy-CardFile (Join-Path 'frogui' $_.Name) }
foreach ($directory in @('frogui\fonts','frogui\sounds','frogui\wallpapers')) {
    if (Test-Path -LiteralPath (Join-Path $card $directory)) { Copy-CardDirectory $directory }
}
@(
    'theme=Switch Dark',
    'background=Theme / artwork',
    'font_size=100',
    'brightness=75',
    'screen_timeout=30',
    'volume=50',
    'ui_sound_pack=Classic',
    'r36sx_display_glitch_fix=off',
    'disable_sleep=on'
) | Set-Content -Encoding ascii (Join-Path $sdRoot 'frogui\settings.txt')

# Rockbox application data is not game content despite its upstream path.
Copy-CardDirectory 'roms\rockbox\.rockbox'
$rockboxConfig = Join-Path $sdRoot 'roms\rockbox\.rockbox\config.cfg'
Get-ChildItem -LiteralPath (Join-Path $sdRoot 'roms\rockbox\.rockbox') -Recurse -File |
    Where-Object { $_.Name -match '\.pre-' } |
    Remove-Item -Force
if (Test-Path -LiteralPath (Join-Path $repoRoot 'apps\rockbox-config.cfg')) {
    Copy-Item -Force -LiteralPath (Join-Path $repoRoot 'apps\rockbox-config.cfg') -Destination $rockboxConfig
}
$rockboxShortcuts = Join-Path $sdRoot 'roms\rockbox\.rockbox\shortcuts.txt'
if (Test-Path -LiteralPath (Join-Path $repoRoot 'apps\rockbox-shortcuts.txt')) {
    Copy-Item -Force -LiteralPath (Join-Path $repoRoot 'apps\rockbox-shortcuts.txt') -Destination $rockboxShortcuts
}

New-Item -ItemType Directory -Force -Path (Join-Path $sdRoot 'MD') | Out-Null
[IO.File]::WriteAllText((Join-Path $sdRoot 'MD\dummy.md'), 'TF', [Text.Encoding]::ASCII)
[IO.File]::WriteAllText((Join-Path $sdRoot 'MD\filelist.csv'), "dummy.md,SwitchFrogUI,MD`n", [Text.Encoding]::ASCII)

$musicDirectory = Join-Path $sdRoot 'Music\SwitchFrogUI Samples'
New-Item -ItemType Directory -Force -Path $musicDirectory | Out-Null
Copy-Item -Force -LiteralPath (Join-Path $repoRoot 'apps\assets\SwitchFrogUI Sample - Mozart - Piano Sonata No. 14.ogg') -Destination $musicDirectory

$jsdevDirectory = Join-Path $sdRoot 'roms\JSDev'
New-Item -ItemType Directory -Force -Path $jsdevDirectory | Out-Null
Copy-Item -Force -LiteralPath (Join-Path $repoRoot 'apps\jsdev\examples\JSDev API Showcase.js') -Destination $jsdevDirectory

$ebookDirectory = Join-Path $sdRoot 'Ebooks\SwitchFrogUI Samples'
New-Item -ItemType Directory -Force -Path $ebookDirectory | Out-Null
Copy-Item -Force -Path (Join-Path $repoRoot 'apps\assets\ebooks\*') -Destination $ebookDirectory

Copy-Item -Force -LiteralPath (Join-Path $repoRoot 'README-R36SX.md') -Destination (Join-Path $stage 'README.md')
Copy-Item -Force -LiteralPath (Join-Path $repoRoot 'LICENSE.md') -Destination $stage
Copy-Item -Force -LiteralPath (Join-Path $repoRoot 'docs\r36sx\INSTALL.md') -Destination $stage
Copy-Item -Force -LiteralPath (Join-Path $repoRoot 'docs\r36sx\FEATURES.md') -Destination $stage
Copy-Item -Force -LiteralPath (Join-Path $repoRoot 'docs\r36sx\THIRD_PARTY_NOTICES.md') -Destination $stage
Copy-Item -Force -LiteralPath (Join-Path $repoRoot 'docs\r36sx\JSDEV.md') -Destination $stage
Copy-Item -Force -LiteralPath (Join-Path $repoRoot 'cores.md') -Destination (Join-Path $stage 'CORE-SOURCES.md')

$files = Get-ChildItem -LiteralPath $stage -Recurse -File
$forbidden = $files | Where-Object {
    $relative = $_.FullName.Substring($stage.Length).TrimStart('\')
    ($relative -match '(?i)(^|\\)(bios|saves?|states?)(\\|$)') -or
    ($relative -match '(?i)(game_history|favorites|state_playtime|video_subtitle_offsets|log\.txt|\.pre-)') -or
    ($_.Extension -match '(?i)^\.(nes|fds|sfc|smc|gba|gb|gbc|gg|sms|mdx|gen|32x|cue|iso|chd|pbp|sav|srm|state|mcr)$') -or
    (($relative -match '(?i)^SD_ROOT\\roms\\') -and
     ($relative -notmatch '(?i)^SD_ROOT\\roms\\rockbox\\') -and
     ($relative -notmatch '(?i)^SD_ROOT\\roms\\JSDev\\JSDev API Showcase\.js$'))
}
if ($forbidden) {
    $names = $forbidden.FullName -join "`n"
    throw "Publish audit rejected forbidden content:`n$names"
}

$textCandidates = $files | Where-Object { $_.Length -lt 2MB -and $_.Extension -match '(?i)^\.(md|txt|xml|cfg|sh|ps1|c|h)$' }
$secretHits = $textCandidates | Select-String -Pattern 'ghp_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}|BEGIN (RSA|OPENSSH|EC) PRIVATE KEY|password\s*=' -ErrorAction SilentlyContinue
if ($secretHits) { throw 'Publish audit found a possible credential. Review before packaging.' }

$hashLines = $files | Sort-Object FullName | Get-FileHash -Algorithm SHA256 |
    ForEach-Object { "$($_.Hash)  $($_.Path.Substring($stage.Length + 1).Replace('\','/'))" }
$hashLines | Set-Content -Encoding ascii (Join-Path $stage 'SHA256SUMS.txt')
@(
    'PASS: allow-list assembly completed',
    'PASS: no ROM or BIOS patterns detected',
    'PASS: no save/history/log patterns detected',
    'PASS: no common credential patterns detected',
    "Files: $($files.Count)",
    "Generated: $([DateTime]::UtcNow.ToString('u')) UTC"
) | Set-Content -Encoding ascii (Join-Path $stage 'PUBLISH-AUDIT.txt')

Compress-Archive -Path $stage -DestinationPath $zipPath -CompressionLevel Optimal
$zipHash = Get-FileHash -LiteralPath $zipPath -Algorithm SHA256
"$($zipHash.Hash)  $([IO.Path]::GetFileName($zipPath))" | Set-Content -Encoding ascii (Join-Path $outParent 'SHA256SUMS.txt')
Write-Host "Release ready: $zipPath"
