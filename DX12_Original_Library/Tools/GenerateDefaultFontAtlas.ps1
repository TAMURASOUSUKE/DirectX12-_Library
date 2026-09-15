param(
    [string]$PngPath = "$PSScriptRoot\..\Src\Graphics\InternalResource\DefaultFontAtlas.png",
    [string]$IncludePath = "$PSScriptRoot\..\Src\Graphics\InternalResource\DefaultFontBytes.inc",
    [string]$PreviewPath = "$PSScriptRoot\..\Src\Graphics\InternalResource\DefaultFontAtlasPreview.png"
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing

Set-StrictMode -Version Latest

$cellWidth = 16
$cellHeight = 24
$columns = 16
$rows = 16
$atlasWidth = $cellWidth * $columns
$atlasHeight = $cellHeight * $rows

# CP437 bytes 0-31 and 127 are display symbols rather than printable control codes.
$controlCodePoints = @(
    0x0000, 0x263A, 0x263B, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022,
    0x25D8, 0x25CB, 0x25D9, 0x2642, 0x2640, 0x266A, 0x266B, 0x263C,
    0x25BA, 0x25C4, 0x2195, 0x203C, 0x00B6, 0x00A7, 0x25AC, 0x21A8,
    0x2191, 0x2193, 0x2192, 0x2190, 0x221F, 0x2194, 0x25B2, 0x25BC
)

$encoding = [System.Text.Encoding]::GetEncoding(437)

$bitmap = New-Object System.Drawing.Bitmap($atlasWidth, $atlasHeight, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.Clear([System.Drawing.Color]::Transparent)
$graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
$graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceOver

$fontFamily = New-Object System.Drawing.FontFamily('Cascadia Mono SemiBold')
$font = New-Object System.Drawing.Font($fontFamily, 18.0, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
$brush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
$outlineBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(235, 8, 12, 18))
$format = [System.Drawing.StringFormat]::GenericTypographic.Clone()
$format.FormatFlags = $format.FormatFlags -bor [System.Drawing.StringFormatFlags]::MeasureTrailingSpaces -bor [System.Drawing.StringFormatFlags]::NoWrap
$format.Alignment = [System.Drawing.StringAlignment]::Center
$format.LineAlignment = [System.Drawing.StringAlignment]::Center

try {
    for ($code = 0; $code -lt 256; ++$code) {
        if ($code -eq 0 -or $code -eq 32) {
            continue
        }

        if ($code -lt 32) {
            $glyph = [char]$controlCodePoints[$code]
        }
        elseif ($code -eq 127) {
            $glyph = [char]0x2302
        }
        else {
            $glyph = $encoding.GetString([byte[]]@($code))
        }

        $column = $code % $columns
        $row = [Math]::Floor($code / $columns)
        $rect = New-Object System.Drawing.RectangleF(
            [single]($column * $cellWidth),
            [single]($row * $cellHeight - 0.5),
            [single]$cellWidth,
            [single]$cellHeight
        )
        foreach ($offset in @(
            @(-1.0, -1.0), @(0.0, -1.0), @(1.0, -1.0),
            @(-1.0,  0.0),               @(1.0,  0.0),
            @(-1.0,  1.0), @(0.0,  1.0), @(1.0,  1.0)
        )) {
            $outlineRect = New-Object System.Drawing.RectangleF(
                [single]($rect.X + $offset[0]),
                [single]($rect.Y + $offset[1]),
                [single]$rect.Width,
                [single]$rect.Height
            )
            $graphics.DrawString([string]$glyph, $font, $outlineBrush, $outlineRect, $format)
        }
        $graphics.DrawString([string]$glyph, $font, $brush, $rect, $format)
    }

    $pngDirectory = Split-Path -Parent $PngPath
    $includeDirectory = Split-Path -Parent $IncludePath
    if ($pngDirectory) { [System.IO.Directory]::CreateDirectory($pngDirectory) | Out-Null }
    if ($includeDirectory) { [System.IO.Directory]::CreateDirectory($includeDirectory) | Out-Null }

    $bitmap.Save($PngPath, [System.Drawing.Imaging.ImageFormat]::Png)

    $bytes = [System.IO.File]::ReadAllBytes($PngPath)
    $builder = New-Object System.Text.StringBuilder
    for ($i = 0; $i -lt $bytes.Length; ++$i) {
        if (($i % 16) -eq 0) { [void]$builder.Append("`t") }
        [void]$builder.Append(('0x{0:X2}' -f $bytes[$i]))
        if ($i -lt ($bytes.Length - 1)) { [void]$builder.Append(',') }
        if (($i % 16) -eq 15) {
            [void]$builder.AppendLine()
        }
        elseif ($i -lt ($bytes.Length - 1)) {
            [void]$builder.Append(' ')
        }
    }
    if (($bytes.Length % 16) -ne 0) { [void]$builder.AppendLine() }
    [System.IO.File]::WriteAllText($IncludePath, $builder.ToString(), [System.Text.UTF8Encoding]::new($false))

    $preview = New-Object System.Drawing.Bitmap($atlasWidth, $atlasHeight, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $previewGraphics = [System.Drawing.Graphics]::FromImage($preview)
    try {
        $previewGraphics.Clear([System.Drawing.Color]::FromArgb(255, 25, 30, 38))
        $previewGraphics.DrawImageUnscaled($bitmap, 0, 0)
        $preview.Save($PreviewPath, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $previewGraphics.Dispose()
        $preview.Dispose()
    }

    Write-Output "PNG=$PngPath"
    Write-Output "INCLUDE=$IncludePath"
    Write-Output "PREVIEW=$PreviewPath"
    Write-Output "PNG_BYTES=$($bytes.Length)"
}
finally {
    $format.Dispose()
    $outlineBrush.Dispose()
    $brush.Dispose()
    $font.Dispose()
    $fontFamily.Dispose()
    $graphics.Dispose()
    $bitmap.Dispose()
}
