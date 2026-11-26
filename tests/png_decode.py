"""Overeni PNG.

  png_decode.py <png> <x> <y> <r> <g> <b> [<x> <y> <r> <g> <b> ...]
  png_decode.py --sample <png>     (320x240 vzorek z tests/test_png.c)
  png_decode.py --colors <png>     vypise pocet ruznych barev snimku

Dekoduje IDAT pres zlib (filtr 0 na kazdem radku), overuje CRC chunku a pixely.
"""
import struct
import sys
import zlib


def decode(path):
    with open(path, "rb") as f:
        data = f.read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", "bad signature"
    pos = 8
    idat = b""
    w = h = None
    while pos < len(data):
        n, typ = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        crc = struct.unpack(">I", data[pos + 8 + n:pos + 12 + n])[0]
        assert crc == zlib.crc32(typ + body), "bad crc " + typ.decode()
        if typ == b"IHDR":
            w, h, depth, ctype, _, _, _ = struct.unpack(">IIBBBBB", body)
            assert (depth, ctype) == (8, 2)
        elif typ == b"IDAT":
            idat += body
        pos += 12 + n
    raw = zlib.decompress(idat)
    assert len(raw) == h * (1 + 3 * w), "bad raw length"
    rows = []
    for y in range(h):
        row = raw[y * (1 + 3 * w):(y + 1) * (1 + 3 * w)]
        assert row[0] == 0, "filter != 0"
        rows.append(row[1:])
    return w, h, rows


def sample565(x, y):
    return ((x * 31 // 319) << 11) | ((y * 63 // 239) << 5) | ((x + y) & 31)


def rgb(p):
    r, g, b = p >> 11, (p >> 5) & 63, p & 31
    return (r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)


def main():
    if sys.argv[1] == "--sample":
        w, h, rows = decode(sys.argv[2])
        assert (w, h) == (320, 240)
        for x, y in ((0, 0), (319, 0), (0, 239), (319, 239), (160, 120), (77, 201)):
            exp = rgb(sample565(x, y))
            got = tuple(rows[y][3 * x:3 * x + 3])
            assert got == exp, "pixel %d,%d: %r != %r" % (x, y, got, exp)
        return 0
    if sys.argv[1] == "--colors":
        w, h, rows = decode(sys.argv[2])
        print(len({row[i:i + 3] for row in rows for i in range(0, 3 * w, 3)}))
        return 0
    w, h, rows = decode(sys.argv[1])
    v = [int(a) for a in sys.argv[2:]]
    assert len(v) % 5 == 0 and v
    for i in range(0, len(v), 5):
        x, y = v[i], v[i + 1]
        got = tuple(rows[y][3 * x:3 * x + 3])
        assert got == tuple(v[i + 2:i + 5]), "pixel %d,%d: %r" % (x, y, got)
    return 0


if __name__ == "__main__":
    sys.exit(main())
