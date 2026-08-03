param([string]$OutputPath = (Join-Path $PSScriptRoot 'settings.png'))

Add-Type -AssemblyName System.Drawing
$size = 512
$bitmap = [Drawing.Bitmap]::new($size, $size, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [Drawing.Graphics]::FromImage($bitmap)
$graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.Clear([Drawing.Color]::Transparent)

$points = [Drawing.PointF[]]::new(32)
for ($i = 0; $i -lt 32; $i++) {
    $angle = -[Math]::PI / 2 + $i * 2 * [Math]::PI / 32
    $phase = $i % 4
    $radius = if ($phase -eq 0 -or $phase -eq 1) { 218.0 } else { 174.0 }
    $points[$i] = [Drawing.PointF]::new(
        [single](256 + [Math]::Cos($angle) * $radius),
        [single](256 + [Math]::Sin($angle) * $radius))
}

$gear = [Drawing.Drawing2D.GraphicsPath]::new([Drawing.Drawing2D.FillMode]::Alternate)
$gear.AddPolygon($points)
$gear.AddEllipse(163, 163, 186, 186)
$graphics.FillPath([Drawing.Brushes]::White, $gear)

$gear.Dispose()
$graphics.Dispose()
$bitmap.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
$bitmap.Dispose()
