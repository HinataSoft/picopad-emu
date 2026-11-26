"""Measure the dominant tone of a mono 16-bit WAV (headless --wav output).

usage: wav_tone.py file.wav --freq HZ [--tol PCT] [--peak N]

The mean is subtracted, then the frequency is taken from the zero crossings:
(crossings - 1) / 2 periods between the first and the last crossing. The peak
is the largest |sample - mean|. Prints "freq=<Hz> peak=<n> crossings=<n>";
exit code 0 when freq is within tol % of HZ and peak >= N, else 1.
"""
import argparse
import struct
import sys
import wave


def measure(path):
    with wave.open(path, "rb") as w:
        if w.getnchannels() != 1 or w.getsampwidth() != 2:
            raise ValueError("expected mono 16-bit WAV")
        rate = w.getframerate()
        raw = w.readframes(w.getnframes())
    n = len(raw) // 2
    if n == 0:
        raise ValueError("empty WAV")
    samples = struct.unpack("<%dh" % n, raw[: n * 2])
    mean = sum(samples) / n
    x = [s - mean for s in samples]
    peak = max(abs(v) for v in x)
    # sign changes, ignoring exact zeros (they keep the previous sign)
    crossings = []
    sign = 0
    for i, v in enumerate(x):
        s = 1 if v > 0 else (-1 if v < 0 else 0)
        if s == 0:
            continue
        if sign != 0 and s != sign:
            crossings.append(i)
        sign = s
    if len(crossings) < 3:
        return 0.0, peak, len(crossings)
    span = (crossings[-1] - crossings[0]) / rate
    freq = (len(crossings) - 1) / 2 / span
    return freq, peak, len(crossings)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("wav")
    ap.add_argument("--freq", type=float, required=True)
    ap.add_argument("--tol", type=float, default=2.0, help="tolerance [%%]")
    ap.add_argument("--peak", type=float, default=0.0)
    a = ap.parse_args()
    try:
        freq, peak, nc = measure(a.wav)
    except (OSError, ValueError, wave.Error) as e:
        print("wav_tone: %s: %s" % (a.wav, e), file=sys.stderr)
        return 1
    print("freq=%.1f peak=%.0f crossings=%d" % (freq, peak, nc))
    ok = abs(freq - a.freq) <= a.freq * a.tol / 100 and peak >= a.peak
    if not ok:
        print("wav_tone: expected %.1f Hz +- %.1f %%, peak >= %.0f" % (a.freq, a.tol, a.peak),
              file=sys.stderr)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
