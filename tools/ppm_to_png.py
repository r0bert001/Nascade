#!/usr/bin/env python3
"""把 Nascade 截图模式输出的 PPM (P6) 转成 PNG，便于预览。"""

import struct
import sys
import zlib


def read_ppm(path: str):
    with open(path, "rb") as handle:
        if handle.readline().strip() != b"P6":
            raise ValueError("not a binary PPM")
        line = handle.readline()
        while line.startswith(b"#"):
            line = handle.readline()
        width, height = map(int, line.split())
        maximum = int(handle.readline())
        if maximum != 255:
            raise ValueError("only 8-bit PPM is supported")
        data = handle.read(width * height * 3)
        if len(data) != width * height * 3:
            raise ValueError("truncated PPM")
        return width, height, data


def chunk(kind: bytes, payload: bytes) -> bytes:
    return (
        struct.pack(">I", len(payload))
        + kind
        + payload
        + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
    )


def main() -> int:
    if len(sys.argv) != 3:
        print("用法: ppm_to_png.py <input.ppm> <output.png>", file=sys.stderr)
        return 2
    width, height, data = read_ppm(sys.argv[1])
    raw = b"".join(
        b"\x00" + data[y * width * 3 : (y + 1) * width * 3]
        for y in range(height)
    )
    png = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw, 9))
        + chunk(b"IEND", b"")
    )
    with open(sys.argv[2], "wb") as handle:
        handle.write(png)
    print(f"{sys.argv[2]}: {width}x{height}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
