<#
.SYNOPSIS
    Generates assets/OverlayDesk.ico.

.DESCRIPTION
    The icon is drawn rather than shipped as an opaque binary, so it can be reviewed as a diff
    and adjusted without a paint program. It is regenerated only when someone runs this - the
    build does not depend on it, because a build that needs System.Drawing to produce an
    executable would fail on a machine without it.

    The mark is the product: a curved screen behind a scanline pattern, cut by the aperture of
    an optic. The cyan is the same colour the overlay's edit-mode border uses, which is the only
    colour the application already claims as its own.

    Entries up to 64 px are stored as DIBs and 256 as PNG. Windows itself reads PNG at every
    size, but the legacy GDI+ path behind System.Drawing.Icon does not, and neither do several
    resource viewers - so anything that might want to read the icon back gets the format it
    understands, and only the size where PNG actually saves meaningful space uses it.
#>
[CmdletBinding()]
param(
    [string]$OutputPath = (Join-Path $PSScriptRoot '..\assets\OverlayDesk.ico')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

# 16 is the one that has to survive: at that size the scanlines merge, so the silhouette and the
# aperture have to carry it on their own.
$sizes = @(16, 24, 32, 48, 64, 128, 256)

function New-IconBitmap {
    param([int]$Size)

    $bmp = New-Object System.Drawing.Bitmap($Size, $Size,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.InterpolationMode = 'HighQualityBicubic'
    $g.Clear([System.Drawing.Color]::Transparent)

    $s = [double]$Size
    $inset = $s * 0.06
    $body = New-Object System.Drawing.RectangleF($inset, $inset, ($s - 2 * $inset), ($s - 2 * $inset))
    $radius = $s * 0.22

    # Rounded-square body.
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $radius * 2
    $path.AddArc($body.X, $body.Y, $d, $d, 180, 90)
    $path.AddArc(($body.Right - $d), $body.Y, $d, $d, 270, 90)
    $path.AddArc(($body.Right - $d), ($body.Bottom - $d), $d, $d, 0, 90)
    $path.AddArc($body.X, ($body.Bottom - $d), $d, $d, 90, 90)
    $path.CloseFigure()

    $bodyBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        $body,
        [System.Drawing.Color]::FromArgb(255, 26, 30, 38),
        [System.Drawing.Color]::FromArgb(255, 12, 14, 18),
        90.0)
    $g.FillPath($bodyBrush, $path)
    $bodyBrush.Dispose()

    # The screen: inset, and slightly barrelled by drawing it as an ellipse-clipped region so the
    # scanlines read as sitting on curved glass rather than on a flat panel.
    $screenInset = $s * 0.17
    $screen = New-Object System.Drawing.RectangleF(
        $screenInset, $screenInset, ($s - 2 * $screenInset), ($s - 2 * $screenInset))

    $screenPath = New-Object System.Drawing.Drawing2D.GraphicsPath
    $screenPath.AddEllipse($screen)

    $saved = $g.Save()
    $g.SetClip($screenPath)

    $glow = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        $screen,
        [System.Drawing.Color]::FromArgb(255, 40, 150, 205),
        [System.Drawing.Color]::FromArgb(255, 14, 58, 88),
        90.0)
    $g.FillRectangle($glow, $screen)
    $glow.Dispose()

    # Scanlines. Spacing scales with the icon so the pattern reads the same at every size, and
    # below 32 px it is dropped entirely - at that scale it turns into mush and only darkens the
    # mark.
    if ($Size -ge 32) {
        $period = [Math]::Max(2.0, $s / 16.0)
        $lineBrush = New-Object System.Drawing.SolidBrush(
            [System.Drawing.Color]::FromArgb(150, 6, 12, 20))
        for ($y = $screen.Y; $y -lt $screen.Bottom; $y += $period) {
            $g.FillRectangle($lineBrush, $screen.X, $y, $screen.Width, ($period * 0.45))
        }
        $lineBrush.Dispose()
    }

    $g.Restore($saved)

    # Aperture ring: the optic. This is the shape that survives at 16 px.
    $ringWidth = [Math]::Max(1.0, $s * 0.075)
    $ringPen = New-Object System.Drawing.Pen(
        [System.Drawing.Color]::FromArgb(255, 38, 191, 255), $ringWidth)
    $g.DrawEllipse($ringPen, $screen)
    $ringPen.Dispose()

    # Outer stroke, so the mark holds its shape on a light background too.
    $edgePen = New-Object System.Drawing.Pen(
        [System.Drawing.Color]::FromArgb(255, 60, 72, 90), [Math]::Max(1.0, $s * 0.03))
    $g.DrawPath($edgePen, $path)
    $edgePen.Dispose()

    $path.Dispose()
    $screenPath.Dispose()
    $g.Dispose()
    return $bmp
}

# --- Compose the .ico -------------------------------------------------------------------------
#
# ICONDIR, then one ICONDIRENTRY per size, then the PNG payloads. Offsets are only known once
# every payload has been encoded, so the images are built first and the directory written after.

function ConvertTo-IconDib {
    <#
        A 32bpp bottom-up DIB with the doubled height an ICO entry expects: the colour data,
        then a 1bpp AND mask. The mask is all zeroes because the alpha channel is what actually
        cuts the shape - but it has to be present and padded, or the entry is malformed.
    #>
    param([System.Drawing.Bitmap]$Bitmap)

    $w = $Bitmap.Width
    $h = $Bitmap.Height

    $stream = New-Object System.IO.MemoryStream
    $writer = New-Object System.IO.BinaryWriter($stream)

    $maskStride = [int](([Math]::Floor(($w + 31) / 32)) * 4)

    $writer.Write([uint32]40)             # BITMAPINFOHEADER size
    $writer.Write([int32]$w)
    $writer.Write([int32]($h * 2))        # colour data plus mask
    $writer.Write([uint16]1)              # planes
    $writer.Write([uint16]32)             # bits per pixel
    $writer.Write([uint32]0)              # BI_RGB
    $writer.Write([uint32]($w * $h * 4 + $maskStride * $h))
    $writer.Write([int32]0); $writer.Write([int32]0)
    $writer.Write([uint32]0); $writer.Write([uint32]0)

    $rect = New-Object System.Drawing.Rectangle(0, 0, $w, $h)
    $data = $Bitmap.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $row = New-Object byte[] ($w * 4)
        for ($y = $h - 1; $y -ge 0; $y--) {
            $line = [IntPtr]::Add($data.Scan0, $y * $data.Stride)
            [System.Runtime.InteropServices.Marshal]::Copy($line, $row, 0, $row.Length)
            $writer.Write($row)
        }
    } finally {
        $Bitmap.UnlockBits($data)
    }

    $writer.Write((New-Object byte[] ($maskStride * $h)))
    $writer.Flush()
    $bytes = $stream.ToArray()
    $writer.Dispose()
    $stream.Dispose()
    return , $bytes
}

$payloads = @()
foreach ($size in $sizes) {
    $bmp = New-IconBitmap -Size $size
    if ($size -ge 256) {
        $stream = New-Object System.IO.MemoryStream
        $bmp.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
        $bytes = $stream.ToArray()
        $stream.Dispose()
    } else {
        $bytes = ConvertTo-IconDib -Bitmap $bmp
    }
    $bmp.Dispose()
    $payloads += , @{ Size = $size; Bytes = $bytes }
}

$output = New-Object System.IO.MemoryStream
$writer = New-Object System.IO.BinaryWriter($output)

$writer.Write([uint16]0)                  # reserved
$writer.Write([uint16]1)                  # type: icon
$writer.Write([uint16]$payloads.Count)

$offset = 6 + (16 * $payloads.Count)
foreach ($entry in $payloads) {
    # 256 is written as 0 in a single byte, which is the format's way of encoding it.
    $dim = if ($entry.Size -ge 256) { 0 } else { $entry.Size }
    $writer.Write([byte]$dim)             # width
    $writer.Write([byte]$dim)             # height
    $writer.Write([byte]0)                # palette entries: none, it is 32-bit
    $writer.Write([byte]0)                # reserved
    $writer.Write([uint16]1)              # colour planes
    $writer.Write([uint16]32)             # bits per pixel
    $writer.Write([uint32]$entry.Bytes.Length)
    $writer.Write([uint32]$offset)
    $offset += $entry.Bytes.Length
}

foreach ($entry in $payloads) {
    $writer.Write($entry.Bytes)
}

$writer.Flush()
$resolved = [System.IO.Path]::GetFullPath($OutputPath)
[System.IO.File]::WriteAllBytes($resolved, $output.ToArray())
$writer.Dispose()
$output.Dispose()

$sizeList = ($sizes -join ', ')
Write-Host "Wrote $resolved ($sizeList px, $([Math]::Round((Get-Item $resolved).Length / 1KB, 1)) KB)"
