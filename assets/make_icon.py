"""Vẽ assets/app.ico (16..256 px) theo tông XArena: nền đen đỏ, viền vàng kim, timeline hai phe, vạch mẫu xử.

    python assets/make_icon.py
"""
import os
from PIL import Image, ImageDraw

S = 1024                                        # vẽ lớn rồi thu nhỏ cho mịn
HERE = os.path.dirname(os.path.abspath(__file__))


def draw():
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    r = int(S * 0.18)
    # nền: dải màu đỏ đậm -> đen
    bg = Image.new("RGBA", (S, S))
    for y in range(S):
        t = y / S
        bg.paste((int(70 * (1 - t) + 14 * t), int(14 * (1 - t) + 9 * t), int(14 * (1 - t) + 9 * t), 255), (0, y, S, y + 1))
    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle((16, 16, S - 16, S - 16), r, fill=255)
    im.paste(bg, (0, 0), mask)
    d = ImageDraw.Draw(im)
    # timeline: vùng dưới vàng kim, vùng trên đỏ
    x0, x1, yt, yb = 150, S - 150, 200, S - 200
    pts = [0.50, 0.50, 0.48, 0.52, 0.56, 0.55, 0.60, 0.66, 0.70, 0.74]
    poly = [(x0 + (x1 - x0) * i / (len(pts) - 1), yb - (yb - yt) * v) for i, v in enumerate(pts)]
    d.polygon([(x0, yt)] + [(x1, yt)] + poly[::-1], fill=(150, 26, 26, 255))
    d.polygon([(x0, yb)] + poly + [(x1, yb)], fill=(240, 192, 64, 255))
    d.line(poly, fill=(255, 236, 170, 255), width=14)
    # vạch mẫu xử (trắng) + đường 60 %
    xm = x0 + (x1 - x0) * 0.78
    d.line([(xm, yt - 40), (xm, yb + 40)], fill=(255, 255, 255, 255), width=30)
    y60 = yb - (yb - yt) * 0.6
    for xx in range(x0, x1, 70):
        d.line([(xx, y60), (min(xx + 36, x1), y60)], fill=(255, 245, 210, 200), width=10)
    # viền vàng kim
    d.rounded_rectangle((16, 16, S - 16, S - 16), r, outline=(240, 192, 64, 255), width=48)
    d.rounded_rectangle((86, 86, S - 86, S - 86), r - 60, outline=(198, 40, 40, 255), width=14)
    return im


if __name__ == "__main__":
    big = draw()
    sizes = [16, 24, 32, 48, 64, 128, 256]
    big.resize((256, 256), Image.LANCZOS).save(os.path.join(HERE, "app.ico"), sizes=[(s, s) for s in sizes])
    big.resize((256, 256), Image.LANCZOS).save(os.path.join(HERE, "app.png"))
    print("assets/app.ico")
