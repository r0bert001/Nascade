#!/usr/bin/env python3
"""生成 Nascade 的占位应用图标（256x256 PNG，RGBA）。

不依赖 Pillow；使用 4x 超采样绘制简单几何图形。
用法: python tools/make_icon.py <输出.png>
"""

import struct
import sys
import zlib

SIZE = 1024
OUT_SIZE = 300


def make_canvas():
    return bytearray(SIZE * SIZE * 4)


def put(canvas, x, y, color):
    if 0 <= x < SIZE and 0 <= y < SIZE:
        i = (y * SIZE + x) * 4
        canvas[i : i + 4] = bytes(color)


def rounded_rect(canvas, x0, y0, x1, y1, radius, color):
    r2 = radius * radius
    for y in range(max(0, y0), min(SIZE, y1)):
        for x in range(max(0, x0), min(SIZE, x1)):
            dx = 0
            dy = 0
            if x < x0 + radius:
                dx = x0 + radius - x
            elif x > x1 - radius - 1:
                dx = x - (x1 - radius - 1)
            if y < y0 + radius:
                dy = y0 + radius - y
            elif y > y1 - radius - 1:
                dy = y - (y1 - radius - 1)
            if dx * dx + dy * dy <= r2:
                put(canvas, x, y, color)


def rect(canvas, x0, y0, x1, y1, color):
    for y in range(max(0, y0), min(SIZE, y1)):
        for x in range(max(0, x0), min(SIZE, x1)):
            put(canvas, x, y, color)


def downsample(canvas):
    out = bytearray(OUT_SIZE * OUT_SIZE * 4)
    for y in range(OUT_SIZE):
        sy0 = y * SIZE // OUT_SIZE
        sy1 = max(sy0 + 1, (y + 1) * SIZE // OUT_SIZE)
        for x in range(OUT_SIZE):
            sx0 = x * SIZE // OUT_SIZE
            sx1 = max(sx0 + 1, (x + 1) * SIZE // OUT_SIZE)
            r = g = b = a = 0
            count = 0
            for sy in range(sy0, sy1):
                for sx in range(sx0, sx1):
                    i = (sy * SIZE + sx) * 4
                    r += canvas[i]
                    g += canvas[i + 1]
                    b += canvas[i + 2]
                    a += canvas[i + 3]
                    count += 1
            o = (y * OUT_SIZE + x) * 4
            out[o : o + 4] = bytes((r // count, g // count, b // count, a // count))
    return out


def png_chunk(kind, payload):
    return (
        struct.pack(">I", len(payload))
        + kind
        + payload
        + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
    )


def write_png(path, rgba):
    raw = b"".join(
        b"\x00" + rgba[y * OUT_SIZE * 4 : (y + 1) * OUT_SIZE * 4]
        for y in range(OUT_SIZE)
    )
    png = (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", struct.pack(">IIBBBBB", OUT_SIZE, OUT_SIZE, 8, 6, 0, 0, 0))
        + png_chunk(b"IDAT", zlib.compress(raw, 9))
        + png_chunk(b"IEND", b"")
    )
    with open(path, "wb") as handle:
        handle.write(png)


def main():
    if len(sys.argv) != 2:
        print("用法: make_icon.py <output.png>", file=sys.stderr)
        return 2

    canvas = make_canvas()
    bg = (27, 36, 51, 255)
    border = (43, 58, 77, 255)
    accent = (240, 164, 58, 255)
    green = (93, 196, 138, 255)
    blue = (95, 143, 208, 255)
    dark = (16, 22, 30, 255)

    rounded_rect(canvas, 64, 64, 960, 960, 200, border)
    rounded_rect(canvas, 80, 80, 944, 944, 184, bg)

    # Three game cases on a shelf
    rounded_rect(canvas, 300, 360, 420, 748, 24, accent)
    rounded_rect(canvas, 452, 300, 572, 748, 24, green)
    rounded_rect(canvas, 604, 408, 724, 748, 24, blue)

    rect(canvas, 316, 392, 404, 452, dark)
    rect(canvas, 468, 332, 556, 392, dark)
    rect(canvas, 620, 440, 708, 500, dark)

    rounded_rect(canvas, 260, 748, 764, 800, 18, border)
    rounded_rect(canvas, 280, 856, 744, 892, 14, accent)

    rgba = downsample(canvas)
    write_png(sys.argv[1], rgba)
    print(f"icon written: {sys.argv[1]} ({OUT_SIZE}x{OUT_SIZE})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
