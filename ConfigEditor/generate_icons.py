#!/usr/bin/env python3
"""
Generate raster icon files for PicoSignals Config Editor using Pillow.
Creates `ConfigEditor/icons/icon.png` (256x256) and `ConfigEditor/icons/icon.ico`.
Layout: capsule housing with semicircle top/bottom whose centers align with red/green lenses.
"""
import os
import sys
try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print('Pillow is required. Install with: pip install pillow')
    sys.exit(1)

outdir = os.path.join(os.path.dirname(__file__), 'icons')
os.makedirs(outdir, exist_ok=True)

size = 256
img = Image.new('RGBA', (size, size), 'white')
draw = ImageDraw.Draw(img)

# compute sizes
# housing width and capsule radius
hw = int(size * 0.32)
r_caps = hw // 2
hx = (size - hw) // 2

# define lens centers so top semicenter == red, bottom semicenter == green
red_y = int(size * 0.22)
green_y = int(size * 0.66)
hy = red_y - r_caps
hh = (green_y + r_caps) - hy

# (pole removed)

# housing (capsule)
try:
    draw.rounded_rectangle([hx, hy, hx + hw, hy + hh], radius=r_caps, fill='#111111')
except Exception:
    draw.rectangle([hx, hy, hx + hw, hy + hh], fill='#111111')

# lenses: red at red_y, green at green_y, yellow midway; closer together
cx = hx + hw // 2
lens_r = int(size * 0.07)
mid_y = (red_y + green_y) // 2
colors = ['#ff3b30', '#ffcc00', '#4cd964']
for cy, c in zip([red_y, mid_y, green_y], colors):
    draw.ellipse([cx - lens_r, cy - lens_r, cx + lens_r, cy + lens_r], fill=c)

# text area at bottom — use a larger TrueType font if available
text = 'CONFIG'
font_size = max(18, int(size * 0.16))
font = None
font_candidates = [
    '/Library/Fonts/Arial.ttf',
    '/Library/Fonts/Helvetica.ttf',
    '/System/Library/Fonts/SFNSDisplay.ttf',
    '/System/Library/Fonts/Supplemental/Arial.ttf',
]
for fc in font_candidates:
    try:
        font = ImageFont.truetype(fc, font_size)
        break
    except Exception:
        font = None

if font is None:
    try:
        font = ImageFont.truetype('Arial.ttf', font_size)
    except Exception:
        font = ImageFont.load_default()

try:
    tw, th = font.getsize(text)
except Exception:
    try:
        bbox = draw.textbbox((0, 0), text, font=font)
        tw = bbox[2] - bbox[0]
        th = bbox[3] - bbox[1]
    except Exception:
        tw, th = (len(text) * 8, int(font_size * 0.8))

# clear band at bottom for text and draw with stroke for legibility
text_band_top = size - th - 18
draw.rectangle([0, text_band_top, size, size], fill='white')
stroke_w = max(1, int(font_size * 0.08))
try:
    draw.text(((size - tw) // 2, size - th - 10), text, fill='white', font=font, stroke_width=stroke_w, stroke_fill='black')
except TypeError:
    # older Pillow may not support stroke parameters; draw outline manually
    x = (size - tw) // 2
    y = size - th - 10
    for ox, oy in [(-1,0),(1,0),(0,-1),(0,1)]:
        draw.text((x+ox, y+oy), text, fill='black', font=font)
    draw.text((x, y), text, fill='white', font=font)

png_path = os.path.join(outdir, 'icon.png')
ico_path = os.path.join(outdir, 'icon.ico')
img.save(png_path)
img.save(ico_path, format='ICO', sizes=[(256, 256)])
print('Wrote:', png_path, ico_path)
