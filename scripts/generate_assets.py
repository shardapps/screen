"""Deterministic Screen artwork; Python standard library only. No network or fonts."""
from pathlib import Path
import math
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]


def png(size: int) -> bytes:
    samples = 3
    rows = bytearray()
    for y in range(size):
        rows.append(0)
        for x in range(size):
            totals = [0, 0, 0, 0]
            for sy in range(samples):
                for sx in range(samples):
                    u = (x + (sx + 0.5) / samples) / size
                    v = (y + (sy + 0.5) / samples) / size
                    # Midnight aperture: violet rim, ivory crescent and diamond glint.
                    dx = max(abs(u - .5) - .28, 0)
                    dy = max(abs(v - .5) - .28, 0)
                    inside = dx * dx + dy * dy <= .19 ** 2
                    light = max(0, 1-math.hypot(u-.25,v-.16)/.95)
                    rgb = (int(36+40*light),int(29+27*light),int(72+74*light))
                    rim = dx*dx+dy*dy > .177**2
                    if rim:
                        rgb = (int(113+31*light),int(92+29*light),int(206+36*light))
                    moon = math.hypot(u-.46,v-.51)<.258 and math.hypot(u-.574,v-.412)>.238
                    star = abs(u-.728)+abs(v-.276)<.058
                    if moon or star:
                        rgb = (255, 250, 235)
                    color = (*rgb, 255) if inside else (0, 0, 0, 0)
                    for i, value in enumerate(color):
                        totals[i] += value
            rows.extend(round(value / samples ** 2) for value in totals)

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))

    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(bytes(rows), 9)) + chunk(b'IEND', b''))


def main():
    assets = ROOT / 'packaging' / 'Assets'
    assets.mkdir(parents=True, exist_ok=True)
    for name, size in [('Square44x44Logo.png', 44), ('Square150x150Logo.png', 150), ('StoreLogo.png', 50), ('StoreTile300.png', 300)]:
        (assets / name).write_bytes(png(size))
    sizes = [16, 32, 48, 256]
    images = [png(size) for size in sizes]
    offset = 6 + 16 * len(sizes)
    icon = bytearray(struct.pack('<HHH', 0, 1, len(sizes)))
    for size, data in zip(sizes, images):
        icon.extend(struct.pack('<BBBBHHII', size % 256, size % 256, 0, 0, 1, 32, len(data), offset))
        offset += len(data)
    for data in images:
        icon.extend(data)
    (ROOT / 'resources' / 'Screen.ico').write_bytes(icon)


if __name__ == '__main__':
    main()
