<#
.SYNOPSIS
    Generates all Windows, Linux and theme launcher icons from SVG sources.
.DESCRIPTION
    Renders org.prismlauncher.PrismLauncher.svg into:
    - org.prismlauncher.PrismLauncher_256.png (256x256 master PNG with alpha, centered)
    - prismlauncher.ico (multi-resolution ICO: 16, 24, 32, 48, 64, 128, 256)
    - launcher/resources/multimc/scalable/launcher.svg
    - program_info/PrismLauncher.icon/Assets/ (macOS assets)
#>

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir

$svgSource = Join-Path $scriptDir "org.prismlauncher.PrismLauncher.svg"
$masterPng = Join-Path $scriptDir "org.prismlauncher.PrismLauncher_256.png"
$icoTarget = Join-Path $scriptDir "prismlauncher.ico"
$themeSvg = Join-Path $projectRoot "launcher\resources\multimc\scalable\launcher.svg"
$macAssetsDir = Join-Path $scriptDir "PrismLauncher.icon\Assets"

Write-Host "Rendering $svgSource to 256x256 transparent PNG..."

# Generate temporary HTML wrapper to force exact 256x256 viewport and prevent half-rendering
$tempHtml = Join-Path $scriptDir "temp_icon_render.html"
$htmlContent = @"
<!DOCTYPE html>
<html>
<head>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    html, body { width: 256px; height: 256px; overflow: hidden; background: transparent; }
    img { width: 256px; height: 256px; display: block; }
  </style>
</head>
<body>
  <img src="org.prismlauncher.PrismLauncher.svg" />
</body>
</html>
"@
[System.IO.File]::WriteAllText($tempHtml, $htmlContent)

$browser = "C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
if (-not (Test-Path $browser)) {
    $browser = "C:\Program Files\Google\Chrome\Application\chrome.exe"
}

if (Test-Path $browser) {
    Start-Process -FilePath $browser -ArgumentList "--headless --screenshot=`"$masterPng`" --window-size=256,256 --default-background-color=00000000 `"file:///$($tempHtml.Replace('\', '/'))`"" -Wait
    Remove-Item $tempHtml -Force -ErrorAction SilentlyContinue
} else {
    Remove-Item $tempHtml -Force -ErrorAction SilentlyContinue
    Write-Warning "Browser not found for headless SVG rendering, keeping current PNG"
}

# Generate multi-resolution ICO
Write-Host "Building multi-resolution ICO ($icoTarget)..."
[System.Reflection.Assembly]::LoadWithPartialName("System.Drawing") | Out-Null
$src = [System.Drawing.Image]::FromFile($masterPng)
$sizes = @(16, 24, 32, 48, 64, 128, 256)
$pngBuffers = @()

foreach ($s in $sizes) {
    $bmp = New-Object System.Drawing.Bitmap $s, $s
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.DrawImage($src, 0, 0, $s, $s)
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $bytes = $ms.ToArray()
    $pngBuffers += ,$bytes
    $g.Dispose()
    $bmp.Dispose()
    $ms.Dispose()
}

$fs = [System.IO.File]::Create($icoTarget)
$bw = New-Object System.IO.BinaryWriter $fs

$bw.Write([uint16]0)
$bw.Write([uint16]1)
$bw.Write([uint16]$sizes.Count)

$offset = 6 + (16 * $sizes.Count)

for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]
    $data = $pngBuffers[$i]
    $w = if ($s -ge 256) { 0 } else { [byte]$s }
    $h = if ($s -ge 256) { 0 } else { [byte]$s }
    $bw.Write([byte]$w)
    $bw.Write([byte]$h)
    $bw.Write([byte]0)
    $bw.Write([byte]0)
    $bw.Write([uint16]1)
    $bw.Write([uint16]32)
    $bw.Write([uint32]$data.Length)
    $bw.Write([uint32]$offset)
    $offset += $data.Length
}

for ($i = 0; $i -lt $sizes.Count; $i++) {
    $data = $pngBuffers[$i]
    $bw.Write($data)
}

$bw.Close()
$fs.Close()
$src.Dispose()

# Copy to themes and mac assets
Write-Host "Updating launcher theme and asset catalogs..."
Copy-Item $svgSource $themeSvg -Force
Copy-Item $svgSource (Join-Path $scriptDir "org.prismlauncher.PrismLauncher.bigsur.svg") -Force
Copy-Item $svgSource (Join-Path $scriptDir "org.prismlauncher.PrismLauncher.logo.svg") -Force
Copy-Item $svgSource (Join-Path $scriptDir "org.prismlauncher.PrismLauncher.logo-darkmode.svg") -Force
Copy-Item $svgSource (Join-Path $macAssetsDir "rainbow.svg") -Force

Write-Host "All icons generated successfully!" -ForegroundColor Green
