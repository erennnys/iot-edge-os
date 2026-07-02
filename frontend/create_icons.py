import sys
import os

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("Installing Pillow...")
    import subprocess
    subprocess.check_call([sys.executable, "-m", "pip", "install", "pillow"])
    from PIL import Image, ImageDraw, ImageFont

def generate_icon(size, path):
    # Dark background
    img = Image.new('RGB', (size, size), color='#0f172a')
    draw = ImageDraw.Draw(img)
    
    # Blue inner rounded rectangle
    margin = size * 0.1
    draw.rounded_rectangle(
        [margin, margin, size - margin, size - margin],
        radius=size * 0.15,
        fill='#2563eb'
    )
    
    # Save icon
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path)
    print(f"Generated icon: {path} ({size}x{size})")

generate_icon(192, 'public/pwa-192x192.png')
generate_icon(512, 'public/pwa-512x512.png')
generate_icon(180, 'public/apple-touch-icon.png')
