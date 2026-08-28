param(
    [Parameter(Mandatory = $true)][string]$CardFilesRoot,
    [Parameter(Mandatory = $true)][string]$OutputArchive
)

$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath $CardFilesRoot).Path
$stageParent = Join-Path ([IO.Path]::GetTempPath()) ('switchfrog-update-' + [guid]::NewGuid().ToString('N'))
$stage = Join-Path $stageParent 'SwitchFrogUI-update'
$payload = Join-Path $stage 'payload'
New-Item -ItemType Directory -Force -Path $payload | Out-Null

try {
    foreach ($top in @('cubegm','frogui')) {
        $from = Join-Path $source $top
        if (Test-Path -LiteralPath $from) {
            Copy-Item -LiteralPath $from -Destination $payload -Recurse -Force
        }
    }

    $protected = @(
        'frogui/settings.txt','frogui/favorites.txt','frogui/playtime.txt',
        'frogui/recent_games.txt','frogui/keyboard_gamepad.cfg','frogui/home_selection.cfg'
    )
    foreach ($relative in $protected) {
        $target = Join-Path $payload ($relative -replace '/', [IO.Path]::DirectorySeparatorChar)
        if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Force }
    }

    $lines = foreach ($file in Get-ChildItem -LiteralPath $payload -Recurse -File | Sort-Object FullName) {
        $relative = $file.FullName.Substring($stage.Length + 1).Replace('\','/')
        $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        "$hash  $relative"
    }
    # H.OS validates this with BusyBox sha256sum. Force Unix line endings so a
    # Windows build host cannot append a literal CR to every manifest filename.
    $manifestText = ($lines -join "`n") + "`n"
    [IO.File]::WriteAllText((Join-Path $stage 'manifest.sha256'), $manifestText, [Text.Encoding]::ASCII)

    $archiveParent = Split-Path -Parent $OutputArchive
    if ($archiveParent) { New-Item -ItemType Directory -Force -Path $archiveParent | Out-Null }
    if (Test-Path -LiteralPath $OutputArchive) { Remove-Item -LiteralPath $OutputArchive -Force }
    tar -czf $OutputArchive -C $stageParent 'SwitchFrogUI-update'
    if ($LASTEXITCODE -ne 0) { throw "tar failed with exit code $LASTEXITCODE" }
    Get-Item -LiteralPath $OutputArchive
} finally {
    if (Test-Path -LiteralPath $stageParent) { Remove-Item -LiteralPath $stageParent -Recurse -Force }
}
