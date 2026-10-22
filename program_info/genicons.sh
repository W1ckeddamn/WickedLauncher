#!/bin/bash
set -e

LAUNCHER_APPID="org.prismlauncher.PrismLauncher"
LAUNCHER_NAME="prismlauncher"

svg2png() {
    input_file="$1"
    output_file="$2"
    width="$3"
    height="$4"

    if command -v "inkscape" &>/dev/null; then
        inkscape -w "$width" -h "$height" -o "$output_file" "$input_file"
    elif command -v "magick" &>/dev/null; then
        magick -background none -density 384 "$input_file" -resize "${width}x${height}" "$output_file"
    elif command -v "convert" &>/dev/null; then
        convert -background none -density 384 "$input_file" -resize "${width}x${height}" "$output_file"
    else
        echo "ERROR: No suitable SVG rasterizer found (inkscape or ImageMagick required)" >&2
        return 1
    fi
}

echo "Generating icons from ${LAUNCHER_APPID}.svg..."

# Windows ICO
if (command -v "inkscape" &>/dev/null || command -v "magick" &>/dev/null || command -v "convert" &>/dev/null) && command -v "icotool" &>/dev/null; then
    d=$(mktemp -d)

    svg2png "${LAUNCHER_APPID}.svg" "$d/${LAUNCHER_NAME}_16.png" 16 16
    svg2png "${LAUNCHER_APPID}.svg" "$d/${LAUNCHER_NAME}_24.png" 24 24
    svg2png "${LAUNCHER_APPID}.svg" "$d/${LAUNCHER_NAME}_32.png" 32 32
    svg2png "${LAUNCHER_APPID}.svg" "$d/${LAUNCHER_NAME}_48.png" 48 48
    svg2png "${LAUNCHER_APPID}.svg" "$d/${LAUNCHER_NAME}_64.png" 64 64
    svg2png "${LAUNCHER_APPID}.svg" "$d/${LAUNCHER_NAME}_128.png" 128 128
    svg2png "${LAUNCHER_APPID}.svg" "$d/${LAUNCHER_NAME}_256.png" 256 256

    if command -v "oxipng" &>/dev/null; then
        oxipng --opt max --strip all --alpha --interlace 0 "$d/${LAUNCHER_NAME}_"*".png" || true
    fi

    rm -f "${LAUNCHER_NAME}.ico"
    icotool -o "${LAUNCHER_NAME}.ico" -c \
        "$d/${LAUNCHER_NAME}_256.png"  \
        "$d/${LAUNCHER_NAME}_128.png"  \
        "$d/${LAUNCHER_NAME}_64.png"   \
        "$d/${LAUNCHER_NAME}_48.png"   \
        "$d/${LAUNCHER_NAME}_32.png"   \
        "$d/${LAUNCHER_NAME}_24.png"   \
        "$d/${LAUNCHER_NAME}_16.png"

    cp -f "$d/${LAUNCHER_NAME}_256.png" "${LAUNCHER_APPID}_256.png"
    rm -rf "$d"
    echo "Successfully generated ${LAUNCHER_NAME}.ico and ${LAUNCHER_APPID}_256.png"
else
    echo "WARNING: Windows icons not generated automatically. Keeping existing ${LAUNCHER_NAME}.ico" >&2
fi

# macOS ICNS
if (command -v "inkscape" &>/dev/null || command -v "magick" &>/dev/null) && command -v "iconutil" &>/dev/null; then
    d=$(mktemp -d)
    iconset="$d/${LAUNCHER_NAME}.iconset"
    mkdir -p "$iconset"

    svg2png "${LAUNCHER_APPID}.bigsur.svg" "$iconset/icon_16x16.png" 16 16
    svg2png "${LAUNCHER_APPID}.bigsur.svg" "$iconset/icon_16x16@2x.png" 32 32
    svg2png "${LAUNCHER_APPID}.bigsur.svg" "$iconset/icon_32x32.png" 32 32
    svg2png "${LAUNCHER_APPID}.bigsur.svg" "$iconset/icon_32x32@2x.png" 64 64
    svg2png "${LAUNCHER_APPID}.bigsur.svg" "$iconset/icon_128x128.png" 128 128
    svg2png "${LAUNCHER_APPID}.bigsur.svg" "$iconset/icon_128x128@2x.png" 256 256
    svg2png "${LAUNCHER_APPID}.bigsur.svg" "$iconset/icon_256x256.png" 256 256
    svg2png "${LAUNCHER_APPID}.bigsur.svg" "$iconset/icon_256x256@2x.png" 512 512
    svg2png "${LAUNCHER_APPID}.bigsur.svg" "$iconset/icon_512x512.png" 512 512
    svg2png "${LAUNCHER_APPID}.bigsur.svg" "$iconset/icon_512x512@2x.png" 1024 1024

    if command -v "oxipng" &>/dev/null; then
        oxipng --opt max --strip all --alpha --interlace 0 "$iconset/icon_"*".png" || true
    fi

    iconutil -c icns "$iconset"
    cp -vf "$d/${LAUNCHER_NAME}.icns" .
    rm -rf "$d"
    echo "Successfully generated ${LAUNCHER_NAME}.icns"
else
    echo "WARNING: macOS icns not generated (requires macOS iconutil)." >&2
fi

# replace icon in themes
mkdir -p "../launcher/resources/multimc/scalable"
cp -vf "${LAUNCHER_APPID}.svg" "../launcher/resources/multimc/scalable/launcher.svg"
echo "All launcher branding assets updated."
