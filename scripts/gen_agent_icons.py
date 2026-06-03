import struct, io, os, sys
from PIL import Image, ImageDraw

OUT_DIR = r"D:\ws\Ecode\images\24x24-free-application-icons\24x24-free-application-icons\ico"

COLORS = {
    "agent-gray":   (0x99, 0x99, 0x99),
    "agent-blue":   (0x4A, 0x90, 0xD9),
    "agent-green":  (0x50, 0xC8, 0x64),
    "agent-orange": (0xF5, 0xA6, 0x23),
    "agent-purple": (0x9B, 0x59, 0xB6),
    "agent-teal":   (0x1A, 0xBC, 0x9C),
    "agent-red":    (0xE7, 0x4C, 0x3C),
    "agent-pink":   (0xE9, 0x1E, 0x63),
    "agent-yellow": (0xF1, 0xC4, 0x0F),
    "agent-cyan":   (0x00, 0xBC, 0xD4),
    "agent-lime":   (0x8B, 0xC3, 0x4A),
    "agent-brown":  (0x79, 0x55, 0x48),
}

def create_circle_ico(name, rgb, sizes=[(24,24), (16,16)]):
    images = []
    for w, h in sizes:
        img = Image.new("RGBA", (w, h), (0,0,0,0))
        draw = ImageDraw.Draw(img)
        cx, cy = w//2, h//2
        r = min(w, h)//2 - 1
        draw.ellipse([cx-r, cy-r, cx+r, cy+r], fill=rgb + (255,))
        images.append(img)

    buf = io.BytesIO()
    images[0].save(buf, format="ICO", append_images=images[1:] if len(images)>1 else None)
    ico_path = os.path.join(OUT_DIR, f"{name}.ico")
    with open(ico_path, "wb") as f:
        f.write(buf.getvalue())
    print(f"Created {ico_path}")
    return ico_path

def create_msg_icon(name, sizes=[(24,24), (16,16)]):
    images = []
    for w, h in sizes:
        img = Image.new("RGBA", (w, h), (0,0,0,0))
        draw = ImageDraw.Draw(img)
        # Chat bubble shape
        cx, cy = w//2, h//2
        bw, bh = w-2, h-2
        r = 3
        draw.rounded_rectangle([1, 1, bw-1, bh-1], radius=r, fill=(0x4A, 0x90, 0xD9, 255))
        # Inner white area
        draw.rounded_rectangle([4, 4, bw-8, bh-8], radius=2, fill=(255,255,255,255))
        # Three dots
        dot_r = 1.5
        for dx in [-4, 0, 4]:
            draw.ellipse([cx+dx-dot_r, cy-dot_r, cx+dx+dot_r, cy+dot_r], fill=(0x4A, 0x90, 0xD9, 255))
        images.append(img)

    buf = io.BytesIO()
    images[0].save(buf, format="ICO", append_images=images[1:] if len(images)>1 else None)
    ico_path = os.path.join(OUT_DIR, f"{name}.ico")
    with open(ico_path, "wb") as f:
        f.write(buf.getvalue())
    print(f"Created {ico_path}")

# Create agent colored circle icons
for name, rgb in COLORS.items():
    create_circle_ico(name, rgb)

# Create LocalMsg app icon
create_msg_icon("localmsg")

print("Done!")
