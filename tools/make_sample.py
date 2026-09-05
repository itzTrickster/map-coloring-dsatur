from pathlib import Path
import struct

WIDTH = 160
HEIGHT = 120
BORDER = 3
OUT = Path(__file__).resolve().parents[1] / "samples" / "map1.bmp"


def is_black(x: int, y: int) -> bool:
    if x < BORDER or y < BORDER or x >= WIDTH - BORDER or y >= HEIGHT - BORDER:
        return True

    lines = (
        abs(x - (40 + y // 3)) <= 1,
        abs(x - (95 - y // 4)) <= 1 and y < 90,
        abs(y - (45 + x // 6)) <= 1 and x > 45,
        abs(y - (92 - x // 10)) <= 1 and x < 120,
    )
    return any(lines)


def build_bmp() -> bytes:
    row_raw = WIDTH * 3
    row_size = (row_raw + 3) & ~3
    image_size = row_size * HEIGHT
    pixel_offset = 14 + 40
    file_size = pixel_offset + image_size

    file_header = struct.pack("<2sIHHI", b"BM", file_size, 0, 0, pixel_offset)
    info_header = struct.pack(
        "<IiiHHIIiiII",
        40,
        WIDTH,
        HEIGHT,
        1,
        24,
        0,
        image_size,
        0,
        0,
        0,
        0,
    )

    rows = bytearray()
    padding = b"\x00" * (row_size - row_raw)

    for y in range(HEIGHT - 1, -1, -1):
        for x in range(WIDTH):
            value = 0 if is_black(x, y) else 255
            rows.extend((value, value, value))
        rows.extend(padding)

    return file_header + info_header + rows


def main() -> None:
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_bytes(build_bmp())
    print(f"Created: {OUT}")


if __name__ == "__main__":
    main()
