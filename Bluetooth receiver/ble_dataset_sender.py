"""Send training_set samples over Bluetooth (SPP serial or BLE write) at 300 Hz.

- Only rows with Class == 1 in training_set.xlsx are transmitted.
- Rows with Class == 0 are skipped but their time span is preserved (no data sent).
- Samples are signed 16-bit little endian integers.
"""

import argparse
import asyncio
import time
from pathlib import Path
from typing import Generator, Optional, Tuple

import pandas as pd
from scipy.io import loadmat

try:
    import serial  # type: ignore
except ImportError:  # pragma: no cover
    serial = None

try:
    from bleak import BleakClient  # type: ignore
except ImportError:  # pragma: no cover
    BleakClient = None


def iter_samples(root: Path, labels_path: Path) -> Generator[Tuple[Optional[float], int], None, None]:
    df = pd.read_excel(labels_path)
    for _, row in df.iterrows():
        record = str(row["Records"])
        cls = int(row["Class"])
        mat_path = root / f"{record}.mat"
        signal = loadmat(mat_path)["val"][0]
        if cls == 0:
            yield None, len(signal)
            continue
        for value in signal:
            yield float(value), 1


def send_over_serial(port: str, baudrate: int, root: Path, labels: Path, sample_rate: float) -> None:
    if serial is None:
        raise RuntimeError("pyserial is required for serial mode (pip install pyserial)")
    interval = 1.0 / sample_rate
    # serial_for_url allows using loop:// when no virtual COM driver is available.
    with serial.serial_for_url(port, baudrate, timeout=1) as ser:
        for value, span in iter_samples(root, labels):
            if value is None:
                time.sleep(span * interval)
                continue
            ser.write(int(value).to_bytes(2, "little", signed=True))
            time.sleep(interval)


async def send_over_ble(address: str, char_uuid: str, root: Path, labels: Path, sample_rate: float, no_response: bool) -> None:
    if BleakClient is None:
        raise RuntimeError("bleak is required for BLE mode (pip install bleak)")
    interval = 1.0 / sample_rate
    async with BleakClient(address) as client:
        for value, span in iter_samples(root, labels):
            if value is None:
                await asyncio.sleep(span * interval)
                continue
            data = int(value).to_bytes(2, "little", signed=True)
            await client.write_gatt_char(char_uuid, data, response=not no_response)
            await asyncio.sleep(interval)


def main() -> None:
    parser = argparse.ArgumentParser(description="Send training_set samples via Bluetooth at 300 Hz")
    parser.add_argument("--mode", choices=["serial", "ble"], required=True, help="serial (Bluetooth SPP) or ble (GATT write)")
    parser.add_argument("--port", help="Serial port for SPP, e.g., COM7 or /dev/rfcomm0")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baudrate (default: 115200)")
    parser.add_argument("--address", help="BLE peripheral address/UUID")
    parser.add_argument("--char", help="BLE characteristic UUID to write samples to")
    parser.add_argument("--sample-rate", type=float, default=300.0, help="Sample rate in Hz (default: 300)")
    parser.add_argument("--labels", default="training_set.xlsx", help="Path to labels Excel (default: training_set.xlsx)")
    parser.add_argument("--root", default="training_set", help="Directory containing .mat files (default: training_set)")
    parser.add_argument("--no-response", action="store_true", help="Use write without response for BLE")
    parser.add_argument("--loop", action="store_true", help="Repeat dataset indefinitely")
    args = parser.parse_args()

    root = Path(args.root)
    labels = Path(args.labels)

    while True:
        if args.mode == "serial":
            if not args.port:
                raise SystemExit("--port is required for serial mode")
            send_over_serial(args.port, args.baud, root, labels, args.sample_rate)
        else:
            if not args.address or not args.char:
                raise SystemExit("--address and --char are required for ble mode")
            asyncio.run(send_over_ble(args.address, args.char, root, labels, args.sample_rate, args.no_response))
        if not args.loop:
            break


if __name__ == "__main__":
    main()
