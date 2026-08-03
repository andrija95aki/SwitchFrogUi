param(
    [Parameter(Mandatory = $true)][string]$SourcePng,
    [Parameter(Mandatory = $true)][string]$OutputBmp
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$width = 640
$height = 480
$source = [System.Drawing.Image]::FromFile((Resolve-Path -LiteralPath $SourcePng))
$canvas = New-Object System.Drawing.Bitmap($width, $height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [System.Drawing.Graphics]::FromImage($canvas)
try {
    $graphics.Clear([System.Drawing.Color]::Black)
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $scale = [Math]::Min($width / $source.Width, $height / $source.Height)
    $drawWidth = [int][Math]::Round($source.Width * $scale)
    $drawHeight = [int][Math]::Round($source.Height * $scale)
    $drawX = [int](($width - $drawWidth) / 2)
    $drawY = [int](($height - $drawHeight) / 2)
    $graphics.DrawImage($source, $drawX, $drawY, $drawWidth, $drawHeight)

    # The affected physical band is x=0..109.  Keep it perfectly black so the
    # firmware-stage static frame cannot expose a seam before FrogUI takes over.
    $graphics.FillRectangle([System.Drawing.Brushes]::Black, 0, 0, 110, $height)
}
finally {
    $graphics.Dispose()
    $source.Dispose()
}

$rect = New-Object System.Drawing.Rectangle(0, 0, $width, $height)
$data = $canvas.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
                         [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
try {
    $stride = [Math]::Abs($data.Stride)
    $pixels = New-Object byte[] ($stride * $height)
    [Runtime.InteropServices.Marshal]::Copy($data.Scan0, $pixels, 0, $pixels.Length)
}
finally {
    $canvas.UnlockBits($data)
    $canvas.Dispose()
}

$parent = Split-Path -Parent $OutputBmp
if ($parent -and -not (Test-Path -LiteralPath $parent)) {
    New-Item -ItemType Directory -Path $parent | Out-Null
}

# H.OS expects an uncompressed, 32-bit, top-down BMP: 54-byte BITMAPINFOHEADER
# followed by exactly 640*480 BGRA pixels.
$stream = [IO.File]::Open($OutputBmp, [IO.FileMode]::Create, [IO.FileAccess]::Write)
$writer = New-Object IO.BinaryWriter($stream)
try {
    $imageBytes = $width * $height * 4
    $writer.Write([byte][char]'B'); $writer.Write([byte][char]'M')
    $writer.Write([int](54 + $imageBytes))
    $writer.Write([int]0)
    $writer.Write([int]54)
    $writer.Write([int]40)
    $writer.Write([int]$width)
    $writer.Write([int](-$height))
    $writer.Write([int16]1)
    $writer.Write([int16]32)
    $writer.Write([int]0)
    $writer.Write([int]$imageBytes)
    $writer.Write([int]0); $writer.Write([int]0)
    $writer.Write([int]0); $writer.Write([int]0)
    for ($y = 0; $y -lt $height; $y++) {
        $writer.Write($pixels, $y * $stride, $width * 4)
    }
}
finally {
    $writer.Dispose()
    $stream.Dispose()
}

Write-Host "Created H.OS R36SX boot logo: $OutputBmp"
