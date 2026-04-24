"""SSE server: receive samples from BLE or serial (including virtual serial) and stream to the web UI.

Usage (BLE auto-scan — recommended):
  python test.py                                              # Auto-scan for Nordic_UART_Service
  python test.py --hz 300                                     # With explicit sample rate

Usage (BLE manual address):
  python test.py --ble-address <MAC_OR_UUID>                  # Uses NUS TX UUID by default
  python test.py --ble-address <MAC_OR_UUID> --ble-char <CHAR_UUID>

Usage (serial / virtual serial):
  python test.py --serial-port COM5 --hz 300
  python test.py --serial-port socketserver://5555 --hz 300   # TCP virtual serial server
  python test.py --serial-port loop:// --hz 300               # pyserial loopback (single-process demo)

Endpoints:
  /            -> serves index.html
  /events      -> SSE stream emitting JSON {"t": <epoch>, "v": <float>}
"""

import asyncio
import argparse
import csv
import json
import os
import queue
import threading
import time
import traceback
from http import HTTPStatus
from http.server import HTTPServer, SimpleHTTPRequestHandler
from pathlib import Path
from socketserver import ThreadingMixIn
from typing import Callable, Optional, Tuple

try:
    from bleak import BleakClient, BleakScanner  # type: ignore
except ImportError:  # optional
    BleakClient = None
    BleakScanner = None
try:
    import serial  # type: ignore
except ImportError:  # optional
    serial = None


BLE_STREAM_PKT_VERSION = 1
BLE_STREAM_PKT_START = 0xA1
BLE_STREAM_PKT_DATA = 0xA2
BLE_STREAM_PKT_END = 0xA3

# Nordic UART Service UUIDs
NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_TX_CHAR_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  # Notify (peripheral → central)


async def scan_for_nus_device(target_name: str = "Nordic_UART_Service", timeout: float = 10.0) -> Optional[str]:
    """Scan for BLE devices advertising NUS service. Returns the address of the matching device."""
    if BleakScanner is None:
        print("[SCAN] bleak is not installed; cannot scan.")
        return None

    print(f"[SCAN] Scanning for BLE device '{target_name}' (timeout={timeout}s)...")
    try:
        devices = await BleakScanner.discover(timeout=timeout)
    except OSError as exc:
        # Windows WinRT scanner often raises "device not ready" when BT radio is off/busy.
        print(f"[SCAN] Scanner start failed: {exc}")
        print("[SCAN] Please check: Bluetooth is ON, airplane mode is OFF, and no app is exclusively using the adapter.")
        print("[SCAN] You can bypass scan with --ble-address <MAC>.")
        return None
    except Exception as exc:
        print(f"[SCAN] Unexpected scanner error: {exc}")
        print("[SCAN] You can bypass scan with --ble-address <MAC>.")
        return None

    def _device_rssi(dev) -> Optional[int]:
        rssi = getattr(dev, "rssi", None)
        if rssi is not None:
            return rssi
        metadata = getattr(dev, "metadata", None)
        if isinstance(metadata, dict):
            md_rssi = metadata.get("rssi")
            if isinstance(md_rssi, (int, float)):
                return int(md_rssi)
        return None

    candidates = []
    for d in devices:
        name = d.name or ""
        rssi = _device_rssi(d)
        rssi_text = f"{rssi} dBm" if rssi is not None else "N/A"
        if target_name.lower() in name.lower():
            candidates.append(d)
            print(f"[SCAN]   Found: {name}  addr={d.address}  RSSI={rssi_text}")

    if not candidates:
        print(f"[SCAN] No device with name '{target_name}' found. All visible devices:")
        for d in devices:
            rssi = _device_rssi(d)
            rssi_text = f"{rssi} dBm" if rssi is not None else "N/A"
            print(f"[SCAN]   {d.name or '(unknown)'}  addr={d.address}  RSSI={rssi_text}")
        return None

    best = max(candidates, key=lambda d: _device_rssi(d) if _device_rssi(d) is not None else -999)
    best_rssi = _device_rssi(best)
    best_rssi_text = f"{best_rssi} dBm" if best_rssi is not None else "N/A"
    print(f"[SCAN] Selected: {best.name}  addr={best.address}  RSSI={best_rssi_text}")
    return best.address


class ThreadingHTTPServer(ThreadingMixIn, HTTPServer):
    daemon_threads = True


class NRF5340Handler(SimpleHTTPRequestHandler):
    """Serve index.html and SSE /events."""

    def log_message(self, fmt: str, *args) -> None:  # type: ignore[override]
        return

    def do_GET(self) -> None:  # type: ignore[override]
        if self.path == "/events":
            self._handle_events()
            return
        if self.path == "/" or self.path.startswith("/index.html"):
            super().do_GET()
            return
        self.send_error(HTTPStatus.NOT_FOUND, "Not Found")

    def _handle_events(self) -> None:
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self.end_headers()

        start = time.time()
        try:
            while True:
                payload = self.server.generate_sample(time.time() - start)
                data = json.dumps(payload)
                message = f"data: {data}\n\n"
                self.wfile.write(message.encode("utf-8"))
                self.wfile.flush()
                time.sleep(1.0 / self.server.hz)
        except (BrokenPipeError, ConnectionResetError):
            return


class NRF5340Server(ThreadingHTTPServer):
    def __init__(
        self,
        server_address: Tuple[str, int],
        handler,
        hz: float,
        sample_fn: Optional[Callable[[float], dict]] = None,
    ):
        super().__init__(server_address, handler)
        self.hz = hz
        self.sample_fn = sample_fn

    def generate_sample(self, elapsed: float):
        if self.sample_fn:
            return self.sample_fn(elapsed)
        return {"t": time.time(), "v": 0.0}


class BluetoothReceiver:
    """Subscribe to a BLE characteristic and push samples into a queue."""

    def __init__(self, address: str, char_uuid: str, out_queue: "queue.SimpleQueue[dict]", reconnect_delay: float = 2.0):
        self.address = address
        self.char_uuid = char_uuid
        self.queue = out_queue
        self.reconnect_delay = reconnect_delay
        self._stop = threading.Event()
        self._thread: Optional[threading.Thread] = None
        self._active_file: Optional[int] = None
        self._expected_samples: int = 0
        self._received_samples: int = 0
        self._connected = False
        self._window_start_time: float = 0.0
        self._window_fs_hz: int = 0
        self._window_integrity: int = 0
        self._window_label: int = 0
        self._window_pkt_count: int = 0
        self._window_data_bytes: int = 0
        self._window_last_progress_time: float = 0.0
        self._window_last_progress_rx: int = 0
        self._window_values = []
        self._window_host_t = []
        self._save_dir = Path(__file__).resolve().parent / "received_data"
        self._save_dir.mkdir(parents=True, exist_ok=True)

    def start(self) -> None:
        if BleakClient is None:
            print("[BLE] bleak is not installed; BLE receiving disabled.")
            return
        if self._thread and self._thread.is_alive():
            return
        self._thread = threading.Thread(target=self._run_loop, daemon=True)
        self._thread.start()
        print(f"[BLE] Starting receiver thread for {self.address} (char={self.char_uuid})")

    def stop(self) -> None:
        self._stop.set()

    @property
    def connected(self) -> bool:
        return self._connected

    def _run_loop(self) -> None:
        asyncio.run(self._runner())

    def _reset_window_state(self) -> None:
        self._active_file = None
        self._expected_samples = 0
        self._received_samples = 0
        self._window_fs_hz = 0
        self._window_integrity = 0
        self._window_label = 0
        self._window_pkt_count = 0
        self._window_data_bytes = 0
        self._window_last_progress_time = 0.0
        self._window_last_progress_rx = 0
        self._window_values = []
        self._window_host_t = []

    def _save_current_window_csv(self, file_id: int, sent_count: int, integrity: int, label: int, reason: str) -> None:
        if not self._window_values:
            return

        rx_count = self._received_samples
        expected = self._expected_samples
        fs_hz = self._window_fs_hz
        suffix = "ok" if rx_count == expected else "partial"
        filename = f"file_{file_id:05d}_n{expected}_rx{rx_count}_label{label}_{suffix}.csv"
        out_path = self._save_dir / filename

        try:
            with out_path.open("w", newline="", encoding="utf-8") as f:
                f.write(
                    f"# file_id={file_id}, expected={expected}, sent_count={sent_count}, "
                    f"rx_count={rx_count}, fs_hz={fs_hz}, integrity={integrity}, label={label}, reason={reason}\n"
                )
                writer = csv.writer(f)
                writer.writerow(["index", "value", "t_rel_s", "t_host_s"])

                for idx in range(len(self._window_values)):
                    value = self._window_values[idx]
                    host_t = self._window_host_t[idx]
                    t_rel = (idx / fs_hz) if fs_hz > 0 else ""
                    writer.writerow([
                        idx,
                        "" if value is None else value,
                        "" if t_rel == "" else f"{t_rel:.6f}",
                        "" if host_t is None else f"{host_t:.6f}",
                    ])

            print(f"[BLE-SAVE] Saved {out_path.name} (reason={reason})")
        except Exception as exc:
            print(f"[BLE-SAVE] Failed to save {out_path}: {exc}")

    async def _runner(self) -> None:
        assert BleakClient is not None  # guarded in start
        while not self._stop.is_set():
            try:
                print(f"[BLE] Connecting to {self.address} ...")
                async with BleakClient(self.address) as client:
                    self._connected = True
                    # Print connection confirmation with device info
                    print(f"[BLE] ===== CONNECTED =====")
                    print(f"[BLE]   Address : {self.address}")
                    print(f"[BLE]   MTU     : {client.mtu_size}")

                    # Enumerate services for verification
                    for service in client.services:
                        if NUS_SERVICE_UUID.lower() in service.uuid.lower():
                            print(f"[BLE]   NUS Service found: {service.uuid}")
                            for char in service.characteristics:
                                props = ", ".join(char.properties)
                                print(f"[BLE]     Char {char.uuid} [{props}]")

                    await client.start_notify(self.char_uuid, self._on_notify)
                    print(f"[BLE]   Subscribed to notifications on {self.char_uuid}")
                    print(f"[BLE] =========================")

                    while not self._stop.is_set():
                        await asyncio.sleep(0.1)

                # If we exit the context manager normally
                self._connected = False
                print(f"[BLE] Disconnected normally.")
            except Exception as exc:
                self._connected = False
                if self._active_file is not None and self._received_samples > 0:
                    print(
                        f"[BLE] Connection dropped during file={self._active_file}, "
                        f"rx={self._received_samples}/{self._expected_samples}. Saving partial file..."
                    )
                    self._save_current_window_csv(
                        self._active_file,
                        self._received_samples,
                        self._window_integrity,
                        self._window_label,
                        reason="disconnect",
                    )
                    self._reset_window_state()

                print(f"[BLE] Connection lost: {exc}. Reconnecting in {self.reconnect_delay}s...")
                print("[BLE] Traceback:\n" + traceback.format_exc())
                await asyncio.sleep(self.reconnect_delay)

    def _on_notify(self, _sender: int, data: bytearray) -> None:
        if not data:
            return

        pkt_type = data[0]
        if len(data) >= 2 and data[1] == BLE_STREAM_PKT_VERSION:
            if pkt_type == BLE_STREAM_PKT_START and len(data) >= 12:
                self._handle_stream_start(data)
                return
            if pkt_type == BLE_STREAM_PKT_DATA and len(data) >= 7:
                self._handle_stream_data(data)
                return
            if pkt_type == BLE_STREAM_PKT_END and len(data) >= 8:
                self._handle_stream_end(data)
                return

        # Backward-compatible fallback: raw int16 sample in first two bytes
        if len(data) >= 2:
            value = int.from_bytes(data[:2], "little", signed=True)
        else:
            value = data[0]
        print(f"[BLE] Raw sample: {value} (len={len(data)} hex={data.hex()})")
        self.queue.put({"t": time.time(), "v": float(value)})

    def _handle_stream_start(self, data: bytearray) -> None:
        if len(data) < 12:
            print(f"[BLE-DECODE] START packet too short: len={len(data)} hex={data.hex()}")
            return

        file_id = int.from_bytes(data[2:4], "little", signed=False)
        sample_count = int.from_bytes(data[4:8], "little", signed=False)
        fs_hz = int.from_bytes(data[8:10], "little", signed=False)
        integrity = data[10]
        label = int.from_bytes(data[11:12], "little", signed=True)

        if self._active_file is not None and self._received_samples > 0:
            print(
                f"[BLE-DECODE] New START before previous END. "
                f"Saving previous file={self._active_file} as partial."
            )
            self._save_current_window_csv(
                self._active_file,
                self._received_samples,
                self._window_integrity,
                self._window_label,
                reason="new_start_before_end",
            )

        self._active_file = file_id
        self._expected_samples = sample_count
        self._received_samples = 0
        self._window_start_time = time.time()
        self._window_fs_hz = fs_hz
        self._window_integrity = integrity
        self._window_label = label
        self._window_pkt_count = 0
        self._window_data_bytes = 0
        self._window_last_progress_time = self._window_start_time
        self._window_last_progress_rx = 0
        self._window_values = [None] * sample_count
        self._window_host_t = [None] * sample_count

        print(
            f"[BLE-DECODE] <<< START >>> "
            f"file={file_id} samples={sample_count} fs={fs_hz}Hz "
            f"integrity={integrity}% label={label} "
            f"raw=[{data[:12].hex()}]"
        )

    def _handle_stream_data(self, data: bytearray) -> None:
        if len(data) < 7:
            print(f"[BLE-DECODE] DATA packet too short: len={len(data)} hex={data.hex()}")
            return

        file_id = int.from_bytes(data[2:4], "little", signed=False)
        start_idx = int.from_bytes(data[4:6], "little", signed=False)
        sample_num = data[6]
        expected_len = 7 + sample_num * 2

        if len(data) < expected_len:
            print(
                f"[BLE-DECODE] DATA truncated: "
                f"file={file_id} len={len(data)} expected={expected_len} hex={data.hex()}"
            )
            return

        self._window_pkt_count += 1
        self._window_data_bytes += len(data)
        packet_host_t = time.time()

        for i in range(sample_num):
            pos = 7 + i * 2
            sample_idx = start_idx + i
            value = int.from_bytes(data[pos:pos + 2], "little", signed=True)
            host_t = packet_host_t

            self.queue.put({"t": host_t, "v": float(value), "file": file_id})

            if 0 <= sample_idx < len(self._window_values):
                if self._window_values[sample_idx] is None:
                    self._received_samples += 1
                self._window_values[sample_idx] = value
                self._window_host_t[sample_idx] = host_t
            else:
                print(
                    f"[BLE-DECODE] DATA sample index out of range: "
                    f"file={file_id} idx={sample_idx} expected<{len(self._window_values)}>"
                )

        # Print first DATA packet of each window for debug
        if self._received_samples == sample_num:
            samples_preview = []
            for i in range(min(sample_num, 3)):
                pos = 7 + i * 2
                samples_preview.append(int.from_bytes(data[pos:pos + 2], "little", signed=True))
            print(
                f"[BLE-DECODE] DATA first-pkt file={file_id} start={start_idx} "
                f"n={sample_num} preview={samples_preview} "
                f"pkt_len={len(data)}"
            )

        # Progress every 600 samples
        if self._received_samples % 600 == 0:
            now = time.time()
            elapsed = now - self._window_start_time
            pct = (self._received_samples / self._expected_samples * 100) if self._expected_samples else 0
            avg_sps = (self._received_samples / elapsed) if elapsed > 0 else 0.0
            avg_pps = (self._window_pkt_count / elapsed) if elapsed > 0 else 0.0
            avg_kbps = ((self._window_data_bytes * 8.0) / elapsed / 1000.0) if elapsed > 0 else 0.0

            dt = now - self._window_last_progress_time
            drx = self._received_samples - self._window_last_progress_rx
            inst_sps = (drx / dt) if dt > 0 else 0.0
            fs_match = (avg_sps * 100.0 / self._window_fs_hz) if self._window_fs_hz > 0 else 0.0

            print(
                f"[BLE-DECODE] DATA progress "
                f"file={file_id} rx={self._received_samples}/{self._expected_samples} "
                f"({pct:.0f}%) elapsed={elapsed:.1f}s "
                f"avg={avg_sps:.1f}S/s inst={inst_sps:.1f}S/s "
                f"pkt={avg_pps:.1f}pkt/s payload={avg_kbps:.1f}kbps "
                f"vs_fs={fs_match:.1f}%"
            )

            self._window_last_progress_time = now
            self._window_last_progress_rx = self._received_samples

    def _handle_stream_end(self, data: bytearray) -> None:
        if len(data) < 8:
            print(f"[BLE-DECODE] END packet too short: len={len(data)} hex={data.hex()}")
            return

        file_id = int.from_bytes(data[2:4], "little", signed=False)
        sent_count = int.from_bytes(data[4:6], "little", signed=False)
        integrity = data[6]
        label = int.from_bytes(data[7:8], "little", signed=True)

        elapsed = time.time() - self._window_start_time
        loss = self._expected_samples - self._received_samples
        avg_sps = (self._received_samples / elapsed) if elapsed > 0 else 0.0
        avg_pps = (self._window_pkt_count / elapsed) if elapsed > 0 else 0.0
        avg_kbps = ((self._window_data_bytes * 8.0) / elapsed / 1000.0) if elapsed > 0 else 0.0
        fs_match = (avg_sps * 100.0 / self._window_fs_hz) if self._window_fs_hz > 0 else 0.0

        print(
            f"[BLE-DECODE] <<< END >>> "
            f"file={file_id} sent={sent_count} "
            f"rx={self._received_samples}/{self._expected_samples} "
            f"loss={loss} integrity={integrity}% label={label} "
            f"elapsed={elapsed:.2f}s avg={avg_sps:.1f}S/s "
            f"pkt={avg_pps:.1f}pkt/s payload={avg_kbps:.1f}kbps "
            f"vs_fs={fs_match:.1f}% "
            f"raw=[{data[:8].hex()}]"
        )

        self._save_current_window_csv(file_id, sent_count, integrity, label, reason="end")
        self._reset_window_state()


class SerialReader:
    """Read signed 16-bit integers from a serial port (e.g., Bluetooth SPP or virtual) and push into a queue."""

    def __init__(self, port: str, baudrate: int, out_queue: "queue.SimpleQueue[dict]"):
        self.port = port
        self.baudrate = baudrate
        self.queue = out_queue
        self._stop = threading.Event()
        self._thread: Optional[threading.Thread] = None

    def start(self) -> None:
        if serial is None:
            print("pyserial is not installed; serial receiving disabled.")
            return
        if self._thread and self._thread.is_alive():
            return
        self._thread = threading.Thread(target=self._worker, daemon=True)
        self._thread.start()
        print(f"Starting serial reader on {self.port} @ {self.baudrate}")

    def stop(self) -> None:
        self._stop.set()

    def _worker(self) -> None:
        assert serial is not None
        try:
            if self.port.startswith("socketserver://"):
                self._serve_tcp()
                return
            with serial.serial_for_url(self.port, self.baudrate, timeout=1) as ser:
                while not self._stop.is_set():
                    raw = ser.read(2)
                    if len(raw) < 2:
                        continue
                    value = int.from_bytes(raw, "little", signed=True)
                    self.queue.put({"t": time.time(), "v": float(value)})
        except Exception as exc:
            print(f"Serial reader stopped: {exc}")

    def _serve_tcp(self) -> None:
        """Minimal TCP server to accept one client and stream little-endian int16 samples."""
        import socket

        host_port = self.port.replace("socketserver://", "", 1)
        host, port = ("0.0.0.0", 0)
        if host_port.startswith(":"):
            host_port = host_port[1:]
        if host_port:
            try:
                port = int(host_port)
            except ValueError:
                pass
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind((host, port))
        sock.listen(1)
        print(f"SerialReader TCP server listening on {host}:{port} ...")
        try:
            conn, addr = sock.accept()
            print(f"SerialReader accepted TCP client {addr}")
            conn.settimeout(1.0)
            while not self._stop.is_set():
                try:
                    raw = conn.recv(2)
                    if not raw:
                        break
                    if len(raw) < 2:
                        continue
                    value = int.from_bytes(raw[:2], "little", signed=True)
                    self.queue.put({"t": time.time(), "v": float(value)})
                except socket.timeout:
                    continue
        finally:
            try:
                conn.close()
            except Exception:
                pass
            sock.close()


def make_queue_sampler(data_queue: "queue.SimpleQueue[dict]") -> Callable[[float], dict]:
    def _sample(_elapsed: float) -> dict:
        try:
            payload = data_queue.get_nowait()
            if "t" not in payload:
                payload["t"] = time.time()
            return payload
        except queue.Empty:
            return {"t": time.time(), "v": 0.0}

    return _sample


def serve(
    host: str,
    port: int,
    hz: float,
    ble_address: Optional[str],
    ble_char: Optional[str],
    serial_port: Optional[str],
    serial_baud: int,
) -> None:
    web_root = Path(__file__).resolve().parent
    os.chdir(web_root)

    data_queue: "queue.SimpleQueue[dict]" = queue.SimpleQueue()
    sample_fn = None
    ble_receiver: Optional[BluetoothReceiver] = None
    serial_reader: Optional[SerialReader] = None

    # Default BLE char to NUS TX if not specified
    if ble_char is None:
        ble_char = NUS_TX_CHAR_UUID

    # Auto-scan for NUS device if no address provided (and BLE mode implied)
    if ble_address is None and serial_port is None:
        print("[MAIN] No --ble-address and no --serial-port specified.")
        print("[MAIN] Auto-scanning for Nordic_UART_Service device...")
        try:
            address = asyncio.run(scan_for_nus_device())
        except Exception as exc:
            print(f"[MAIN] BLE auto-scan failed: {exc}")
            address = None
        if address:
            ble_address = address
        else:
            print("[MAIN] No NUS device found. Starting without BLE. Use --ble-address to specify manually.")

    if ble_address:
        ble_receiver = BluetoothReceiver(ble_address, ble_char, data_queue)
        ble_receiver.start()
        sample_fn = make_queue_sampler(data_queue)
    if serial_port:
        serial_reader = SerialReader(serial_port, serial_baud, data_queue)
        serial_reader.start()
        sample_fn = make_queue_sampler(data_queue)

    server = NRF5340Server((host, port), NRF5340Handler, hz, sample_fn=sample_fn)
    print(f"Serving index.html and SSE on http://{host}:{port} (rate {hz} Hz)")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping server...")
        server.server_close()
        if ble_receiver:
            ble_receiver.stop()
        if serial_reader:
            serial_reader.stop()


def main():
    parser = argparse.ArgumentParser(description="Forward BLE/serial samples to SSE for the web UI")
    parser.add_argument("--host", default="127.0.0.1", help="Bind host (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=8000, help="HTTP port (default: 8000)")
    parser.add_argument("--hz", type=float, default=300.0, help="Sample send rate in Hz (default: 300)")
    parser.add_argument("--ble-address", help="BLE peripheral MAC/UUID (omit to auto-scan for Nordic_UART_Service)")
    parser.add_argument("--ble-char", help=f"Characteristic UUID for notifications (default: NUS TX {NUS_TX_CHAR_UUID})")
    parser.add_argument("--serial-port", help="Serial port (Bluetooth SPP or virtual) to read int16 samples from")
    parser.add_argument("--serial-baud", type=int, default=115200, help="Baudrate for serial port (default: 115200)")
    args = parser.parse_args()

    serve(
        args.host,
        args.port,
        args.hz,
        args.ble_address,
        args.ble_char,
        args.serial_port,
        args.serial_baud,
    )


if __name__ == "__main__":
    main()
