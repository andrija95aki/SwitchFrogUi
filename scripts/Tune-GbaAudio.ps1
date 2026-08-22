param(
    [Parameter(Mandatory = $true)]
    [string[]]$CardRoots,
    [string[]]$GamePatterns = @('*Dexter*', '*Salda*', '*Zelda*')
)

$ErrorActionPreference = 'Stop'

foreach ($root in $CardRoots) {
    $resolvedRoot = (Resolve-Path -LiteralPath $root).Path
    $configRoot = Join-Path $resolvedRoot 'picoarch\gba'
    if (-not (Test-Path -LiteralPath $configRoot -PathType Container)) { continue }

    $configs = foreach ($pattern in $GamePatterns) {
        Get-ChildItem -LiteralPath $configRoot -File -Filter ($pattern + '.cfg') -ErrorAction SilentlyContinue
    }
    foreach ($config in $configs | Sort-Object FullName -Unique) {
        $backup = $config.FullName + '.pre-audio-tuning'
        if (-not (Test-Path -LiteralPath $backup)) {
            Copy-Item -LiteralPath $config.FullName -Destination $backup
        }
        $text = Get-Content -LiteralPath $config.FullName -Raw
        $text = $text -replace '(?m)^audio_buffer_size\s*=.*$', 'audio_buffer_size = 7'
        $text = $text -replace '(?m)^gpsp_frameskip\s*=.*$', 'gpsp_frameskip = auto_threshold'
        $text = $text -replace '(?m)^gpsp_frameskip_threshold\s*=.*$', 'gpsp_frameskip_threshold = 40'
        $text = $text -replace '(?m)^gpsp_frameskip_interval\s*=.*$', 'gpsp_frameskip_interval = 1'
        Set-Content -LiteralPath $config.FullName -Value $text -NoNewline
        Write-Host "Tuned $($config.FullName)"
    }
}
