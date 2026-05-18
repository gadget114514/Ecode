
Add-Type -AssemblyName System.Drawing

$pngPath = "$PSScriptRoot\app_icon.png"
$icoPath = "$PSScriptRoot\app_icon.ico"

# Load original
$srcImg = [System.Drawing.Image]::FromFile($pngPath)

# Resize to 256x256
$bmp256 = New-Object System.Drawing.Bitmap(256, 256)
$g = [System.Drawing.Graphics]::FromImage($bmp256)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.DrawImage($srcImg, 0, 0, 256, 256)
$g.Dispose()

# Save 256x256 as PNG stream
$ms = New-Object System.IO.MemoryStream
$bmp256.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
$pngBytes = $ms.ToArray()
$ms.Dispose()
$bmp256.Dispose()
$srcImg.Dispose()

# Write ICO container
$fs = [System.IO.File]::Create($icoPath)
$bw = New-Object System.IO.BinaryWriter($fs)

# ICONDIR
$bw.Write([int16]0) # Reserved
$bw.Write([int16]1) # Type 1=Icon
$bw.Write([int16]1) # Count 1

# ICONDIRENTRY
$bw.Write([byte]0)   # Width 0=256
$bw.Write([byte]0)   # Height 0=256
$bw.Write([byte]0)   # Colors
$bw.Write([byte]0)   # Reserved
$bw.Write([int16]1)  # Planes
$bw.Write([int16]32) # BPP
$bw.Write([int32]$pngBytes.Length) # Size
$bw.Write([int32](6 + 16))         # Offset (Header 6 + Entry 16)

# Data
$bw.Write($pngBytes)

$bw.Close()
$fs.Close()

Write-Host "Created ICO with size: $([System.IO.FileInfo]::new($icoPath).Length)"
