param(
    [string]$InputPng = (Join-Path $PSScriptRoot '..\assets\treefrogui-contributions-boot.png'),
    [string]$OutputPng = (Join-Path $PSScriptRoot '..\assets\switchfrogui-boot.png')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$source = [System.Drawing.Image]::FromFile((Resolve-Path -LiteralPath $InputPng))
$canvas = New-Object System.Drawing.Bitmap($source.Width, $source.Height,
    [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [System.Drawing.Graphics]::FromImage($canvas)
try {
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $graphics.DrawImageUnscaled($source, 0, 0)

    # Preserve the established controller art and replace only the fork name.
    $textTop = [int]($source.Height * 0.65)
    $graphics.FillRectangle([System.Drawing.Brushes]::Black, 0, $textTop,
        $source.Width, $source.Height - $textTop)

    $font = New-Object System.Drawing.Font('Arial', 96,
        [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
    $format = New-Object System.Drawing.StringFormat
    $format.Alignment = [System.Drawing.StringAlignment]::Center
    $format.LineAlignment = [System.Drawing.StringAlignment]::Near
    $center = New-Object System.Drawing.PointF(($source.Width / 2), ($source.Height * 0.70))

    # A restrained cyan halo matches the controller outline without animation.
    foreach ($radius in 10, 7, 4) {
        $alpha = 28 + (10 - $radius) * 7
        $brush = New-Object System.Drawing.SolidBrush(
            [System.Drawing.Color]::FromArgb($alpha, 0, 235, 255))
        try {
            foreach ($dx in (-$radius), 0, $radius) {
                foreach ($dy in (-$radius), 0, $radius) {
                    if ($dx -eq 0 -and $dy -eq 0) { continue }
                    $p = New-Object System.Drawing.PointF(($center.X + $dx), ($center.Y + $dy))
                    $graphics.DrawString('SwitchFrogUI', $font, $brush, $p, $format)
                }
            }
        }
        finally { $brush.Dispose() }
    }
    $graphics.DrawString('SwitchFrogUI', $font, [System.Drawing.Brushes]::White,
        $center, $format)
}
finally {
    if ($format) { $format.Dispose() }
    if ($font) { $font.Dispose() }
    $graphics.Dispose()
    $source.Dispose()
}

$parent = Split-Path -Parent $OutputPng
if ($parent -and -not (Test-Path -LiteralPath $parent)) {
    New-Item -ItemType Directory -Path $parent | Out-Null
}
$canvas.Save($OutputPng, [System.Drawing.Imaging.ImageFormat]::Png)
$canvas.Dispose()
Write-Host "Created SwitchFrogUI boot source: $OutputPng"
