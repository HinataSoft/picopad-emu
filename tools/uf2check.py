"""Checks a Picopad UF2: blocks, family and the loader CRC of the application.

usage: python -I uf2check.py <file.uf2>   (exit 0 = OK, 1 = failure)
"""
import struct
import sys

MAGIC0, MAGIC1, MAGIC_END = 0x0A324655, 0x9E5D5157, 0x0AB16F30
FAMILY = 0xE48BFF56
APP_OFFSET = 0x8000
CRC_START = 204


def crc32_bootrom(data):
    crc = 0xFFFFFFFF
    for b in data:
        crc ^= b << 24
        for _ in range(8):
            crc = ((crc << 1) ^ 0x04C11DB7) & 0xFFFFFFFF if crc & 0x80000000 else (crc << 1) & 0xFFFFFFFF
    return crc


def check(path):
    errors = []
    with open(path, 'rb') as f:
        data = f.read()
    if not data or len(data) % 512:
        return ['size %d is not a multiple of 512' % len(data)]
    payload = bytearray()
    for i in range(len(data) // 512):
        blk = data[i * 512:(i + 1) * 512]
        m0, m1 = struct.unpack_from('<II', blk, 0)
        size, = struct.unpack_from('<I', blk, 16)
        family, = struct.unpack_from('<I', blk, 28)
        end, = struct.unpack_from('<I', blk, 508)
        if m0 != MAGIC0 or m1 != MAGIC1 or end != MAGIC_END:
            errors.append('block %d: bad magic' % i)
        if family != FAMILY:
            errors.append('block %d: family 0x%08X, expected 0x%08X' % (i, family, FAMILY))
        if size > 476:
            errors.append('block %d: payload size %d' % (i, size))
            size = 476
        payload += blk[32:32 + size]
        if len(errors) > 10:
            break
    if errors:
        return errors
    app = bytes(payload[APP_OFFSET:])
    if len(app) < CRC_START + 4:
        return ['application too short (%d bytes after 0x8000)' % len(app)]
    w = lambda n: struct.unpack_from('<I', app, n * 4)[0]
    if w(48) != 0x44415050:
        errors.append('word 48 = 0x%08X, expected 0x44415050 (PPAD)' % w(48))
    if w(53) != 0x64617070:
        errors.append('word 53 = 0x%08X, expected 0x64617070 (ppad)' % w(53))
    length, stored = w(49), w(50)
    if CRC_START + length > len(app):
        errors.append('length %d exceeds the application (%d bytes)' % (length, len(app)))
    else:
        calc = crc32_bootrom(app[CRC_START:CRC_START + length])
        if calc != stored:
            errors.append('CRC stored 0x%08X, computed 0x%08X' % (stored, calc))
    return errors


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 1
    errors = check(sys.argv[1])
    for e in errors:
        print('FAIL: ' + e)
    if not errors:
        print('OK: ' + sys.argv[1])
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main())
