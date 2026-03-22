#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Auto-send all ECG .mat files in ./data over UART as int16 little-endian stream."""

import pathlib
import struct
import time

import numpy as np
import scipy.io as sio
import serial


# Fixed config
PORT = "/dev/ttyUSB0"
BAUDRATE = 115200
FS = 300.0
DATA_DIR = "test_set1"
LOOP = False
VERBOSE_EVERY = 300


def load_ecg_int16(data_dir: pathlib.Path, mat_name: str) -> np.ndarray:
    mat_path = data_dir / mat_name
    if not mat_path.exists():
        raise FileNotFoundError(f"MAT file not found: {mat_path}")

    mat = sio.loadmat(str(mat_path))
    if "val" not in mat:
        raise KeyError(f"Key 'val' not found in {mat_path.name}")

    ecg = mat["val"].flatten()
    ecg_int16 = ecg.astype(np.int16)
    return ecg_int16


def list_mat_files(data_dir: pathlib.Path) -> list[pathlib.Path]:
    if not data_dir.exists():
        raise FileNotFoundError(f"Data directory not found: {data_dir}")

    mat_files = sorted(data_dir.glob("*.mat"))
    if not mat_files:
        raise FileNotFoundError(f"No .mat files found in: {data_dir}")

    return mat_files


def print_mat_file_stats(datasets: list[tuple[str, np.ndarray]]) -> None:
    print("[INFO] MAT file statistics:")
    total_samples = 0
    for idx, (file_name, ecg_int16) in enumerate(datasets, start=1):
        sample_count = int(len(ecg_int16))
        total_samples += sample_count
        print(f"  {idx:02d}. {file_name}: {sample_count} samples")

    print(f"[INFO] Total files: {len(datasets)}, total samples: {total_samples}")


def send_stream(
    port: str,
    baudrate: int,
    datasets: list[tuple[str, np.ndarray]],
    fs: float,
    loop: bool,
    verbose_every: int,
) -> None:
    sample_period = 1.0 / fs

    with serial.Serial(port=port, baudrate=baudrate, timeout=0) as ser:
        print(f"[INFO] Opened UART {port} @ {baudrate}")
        print(f"[INFO] Files: {len(datasets)}, fs={fs} Hz, period={sample_period*1000:.3f} ms")
        print("[INFO] Sending int16 as little-endian 2-byte stream...")

        sent_total = 0
        start_time = time.perf_counter()

        while True:
            for file_index, (file_name, ecg_int16) in enumerate(datasets, start=1):
                print(f"[INFO] Sending file {file_index}/{len(datasets)}: {file_name} ({len(ecg_int16)} samples)")

                for i, sample in enumerate(ecg_int16):
                    frame = struct.pack("<h", int(sample))
                    ser.write(frame)
                    sent_total += 1

                    if verbose_every > 0 and (sent_total % verbose_every == 0):
                        elapsed = time.perf_counter() - start_time
                        rate = sent_total / elapsed if elapsed > 0 else 0.0
                        print(
                            f"[TX] total={sent_total}, file={file_name}, idx={i}, sample={int(sample)}, "
                            f"avg_rate={rate:.1f} samples/s"
                        )

                    time.sleep(sample_period)

            if not loop:
                break

        print(f"[DONE] Sent {sent_total} samples")


def main() -> None:
    data_dir = pathlib.Path(DATA_DIR)
    datasets = []

    for mat_path in list_mat_files(data_dir):
        ecg_int16 = load_ecg_int16(data_dir, mat_path.name)
        datasets.append((mat_path.name, ecg_int16))

    # print_mat_file_stats(datasets)

    send_stream(
        port=PORT,
        baudrate=BAUDRATE,
        datasets=datasets,
        fs=FS,
        loop=LOOP,
        verbose_every=VERBOSE_EVERY,
    )


if __name__ == "__main__":
    main()
