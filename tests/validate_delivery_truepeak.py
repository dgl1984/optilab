#!/usr/bin/env python3
"""Public reference validation for OptiLab Core final delivery.

Requires Python 3, NumPy and SciPy. The script generates deterministic stereo
stress signals, runs the built optilab-core-cli at every advertised sample-rate
family, reconstructs peaks independently with scipy.signal.resample_poly, and
fails if the measured result exceeds -0.09 dBTP.

This is deliberately separate from CTest so the ordinary source build does not
require Python scientific packages.
"""
from __future__ import annotations
import argparse
import csv
import math
import struct
import subprocess
import tempfile
import wave
from pathlib import Path

import numpy as np
from scipy.signal import resample_poly

RATES = [8000,11025,16000,22050,32000,44100,48000,88200,96000,176400,192000,352800,384000,705600,768000]
MODES = ["podcast", "stream", "limiter"]
LIMIT_DBTP = -0.09


def write_pcm16(path: Path, data: np.ndarray, rate: int) -> None:
    data = np.clip(data, -0.999969, 0.999969)
    pcm = np.round(data * 32767.0).astype('<i2')
    with wave.open(str(path), 'wb') as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(pcm.tobytes())


def read_pcm_or_float_wav(path: Path) -> tuple[np.ndarray,int]:
    # The Core CLI writes standard stereo float WAV. scipy is already required,
    # so use scipy.io for format coverage instead of adding another dependency.
    from scipy.io import wavfile
    rate, x = wavfile.read(path)
    if x.ndim == 1:
        x = x[:,None]
    if np.issubdtype(x.dtype, np.integer):
        scale = float(max(abs(np.iinfo(x.dtype).min), np.iinfo(x.dtype).max))
        x = x.astype(np.float64) / scale
    else:
        x = x.astype(np.float64)
    return x, int(rate)


def make_signal(rate: int) -> np.ndarray:
    duration = 0.7 if rate >= 352800 else 1.0
    t = np.arange(int(rate * duration), dtype=np.float64) / rate
    hf = min(5000.0, rate * 0.32)
    left = (0.43*np.sin(2*np.pi*80*t) + 0.24*np.sin(2*np.pi*hf*t)
            + 0.18*np.sin(2*np.pi*440*t) + 0.12*np.sin(2*np.pi*880*t))
    right = (0.42*np.sin(2*np.pi*80*t + 0.41) + 0.23*np.sin(2*np.pi*hf*t + 1.1)
             + 0.18*np.sin(2*np.pi*440*t + 0.7) + 0.12*np.sin(2*np.pi*880*t + 1.4))
    for pos in (0.17, 0.39, 0.61):
        i = int(pos * rate)
        if i < len(left):
            left[i] += 0.95; right[i] -= 0.87
            if i + 1 < len(left):
                left[i+1] -= 0.40; right[i+1] += 0.35
    x = np.stack((left, right), axis=1)
    x *= 0.99 / max(float(np.max(np.abs(x))), 1e-12)
    return x


def reconstructed_peak_db(x: np.ndarray, rate: int) -> tuple[float,int]:
    factor = 16 if rate <= 96000 else (8 if rate <= 192000 else 4)
    peak = 0.0
    for ch in range(x.shape[1]):
        y = resample_poly(x[:,ch], factor, 1, window=('kaiser', 10.5))
        peak = max(peak, float(np.max(np.abs(y))))
    return 20.0 * math.log10(max(peak, 1e-15)), factor


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--cli', required=True, type=Path)
    ap.add_argument('--report', type=Path)
    args = ap.parse_args()
    rows = []
    worst = -999.0
    with tempfile.TemporaryDirectory(prefix='optilab-tp-') as td:
        td = Path(td)
        for rate in RATES:
            inp = td / f'in_{rate}.wav'
            write_pcm16(inp, make_signal(rate), rate)
            for mode in MODES:
                out = td / f'out_{rate}_{mode}.wav'
                subprocess.run([str(args.cli), str(inp), str(out), '--mode', mode,
                                '--adapt', '100', '--input', '0'], check=True,
                               stdout=subprocess.DEVNULL)
                x, actual_rate = read_pcm_or_float_wav(out)
                sample_peak = 20.0 * math.log10(max(float(np.max(np.abs(x))), 1e-15))
                tp, factor = reconstructed_peak_db(x, actual_rate)
                worst = max(worst, tp)
                rows.append((rate, mode, sample_peak, tp, factor))
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        with args.report.open('w', newline='', encoding='utf-8') as f:
            w = csv.writer(f, delimiter='\t')
            w.writerow(['sample_rate','mode','sample_peak_dbfs','approx_true_peak_dbtp','reconstruction_factor'])
            w.writerows(rows)
    print(f'Worst reconstructed peak: {worst:.6f} dBTP')
    if worst > LIMIT_DBTP:
        print(f'FAIL: exceeds {LIMIT_DBTP:.2f} dBTP validation limit')
        return 1
    print('PASS')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
