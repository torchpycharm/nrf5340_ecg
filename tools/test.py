"""SSE server: receive samples from BLE or serial (including virtual serial) and stream to the web UI.

Usage (BLE):
  python test.py --ble-address <MAC_OR_UUID> --ble-char <CHAR_UUID> --hz 300

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
import json
import os
import queue
import threading
import time
from http import HTTPStatus
from http.server import HTTPServer, SimpleHTTPRequestHandler
from pathlib import Path
from socketserver import ThreadingMixIn
from typing import Callable, Optional, Tuple

try:
    from bleak import BleakClient  # type: ignore
except ImportError:  # optional
    BleakClient = None
try:
    import serial  # type: ignore
except ImportError:  # optional
    serial = None


BLE_STREAM_PKT_VERSION = 1
BLE_STREAM_PKT_START = 0xA1
BLE_STREAM_PKT_DATA = 0xA2
BLE_STREAM_PKT_END = 0xA3


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

    def start(self) -> None:
        if BleakClient is None:
            print("bleak is not installed; BLE receiving disabled.")
            return
        if self._thread and self._thread.is_alive():
            return
        self._thread = threading.Thread(target=self._run_loop, daemon=True)
        self._thread.start()
        print(f"Starting BLE receiver thread for {self.address} ({self.char_uuid})")

    def stop(self) -> None:
        self._stop.set()

    def _run_loop(self) -> None:
        asyncio.run(self._runner())

    async def _runner(self) -> None:
        assert BleakClient is not None  # guarded in start
        while not self._stop.is_set():
            try:
                async with BleakClient(self.address) as client:
                    await client.start_notify(self.char_uuid, self._on_notify)
                    while not self._stop.is_set():
                        await asyncio.sleep(0.1)
            except Exception as exc:
                print(f"BLE connection lost: {exc}. Reconnecting in {self.reconnect_delay}s...")
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
        self.queue.put({"t": time.time(), "v": float(value)})

    def _handle_stream_start(self, data: bytearray) -> None:
        if len(data) < 12:
            print(f"BLE START packet too short: len={len(data)}")
            return

        file_id = int.from_bytes(data[2:4], "little", signed=False)
        sample_count = int.from_bytes(data[4:8], "little", signed=False)
        fs_hz = int.from_bytes(data[8:10], "little", signed=False)
        integrity = data[10]
        label = int.from_bytes(data[11:12], "little", signed=True)

        self._active_file = file_id
        self._expected_samples = sample_count
        self._received_samples = 0

        print(
            "BLE START "
            f"file={file_id} samples={sample_count} fs={fs_hz}Hz "
            f"integrity={integrity}% label={label}"
        )

    def _handle_stream_data(self, data: bytearray) -> None:
        if len(data) < 7:
            print(f"BLE DATA packet too short: len={len(data)}")
            return

        file_id = int.from_bytes(data[2:4], "little", signed=False)
        start_idx = int.from_bytes(data[4:6], "little", signed=False)
        sample_num = data[6]
        expected_len = 7 + sample_num * 2

        if len(data) < expected_len:
            print(
                "BLE DATA packet truncated: "
                f"file={file_id} len={len(data)} expected={expected_len}"
            )
            return

        for i in range(sample_num):
            pos = 7 + i * 2
            value = int.from_bytes(data[pos:pos + 2], "little", signed=True)
            self.queue.put({"t": time.time(), "v": float(value), "file": file_id})

        self._received_samples += sample_num
        if self._received_samples % 600 == 0:
            print(
                "BLE DATA progress "
                f"file={file_id} start={start_idx} "
                f"received={self._received_samples}/{self._expected_samples}"
            )

    def _handle_stream_end(self, data: bytearray) -> None:
        if len(data) < 8:
            print(f"BLE END packet too short: len={len(data)}")
            return

        file_id = int.from_bytes(data[2:4], "little", signed=False)
        sent_count = int.from_bytes(data[4:6], "little", signed=False)
        integrity = data[6]
        label = int.from_bytes(data[7:8], "little", signed=True)

        print(
            "BLE END "
            f"file={file_id} sent={sent_count} "
            f"rx={self._received_samples}/{self._expected_samples} "
            f"integrity={integrity}% label={label}"
        )

        self._active_file = None
        self._expected_samples = 0
        self._received_samples = 0


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

    if ble_address and ble_char:
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
    parser.add_argument("--ble-address", help="BLE peripheral MAC/UUID to subscribe for data")
    parser.add_argument("--ble-char", help="Characteristic UUID that notifies samples")
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
