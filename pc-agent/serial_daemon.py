#!/usr/bin/env python3
"""
Serial Daemon — persistent ESP32 connection via TCP socket.
Run in background, then use send.py to push commands.

Usage:
    python serial_daemon.py              # Auto-detect port
    python serial_daemon.py --port COM3  # Specific port
    python serial_daemon.py --port COM3 --baud 115200
"""

import serial
import serial.tools.list_ports
import socket
import threading
import time
import os
import sys
import signal
from datetime import datetime

# ── Config ─────────────────────────────────────────────────────
HOST = "127.0.0.1"
BAUD = 115200

# ── Logging ────────────────────────────────────────────────────
LOG_FILE = None

def log(msg, end="\n"):
    global LOG_FILE
    ts = datetime.now().strftime("%H:%M:%S")
    line = f"[{ts}] {msg}"
    print(line, end=end, flush=True)
    if LOG_FILE is None:
        LOG_FILE = os.path.join(os.path.dirname(__file__), "_serial_daemon.log")
    try:
        with open(LOG_FILE, "a", encoding="utf-8") as f:
            f.write(line + "\n")
    except:
        pass


# ── Serial Manager ─────────────────────────────────────────────
class SerialBridge:
    def __init__(self, port=None, baud=BAUD):
        self.port = port
        self.baud = baud
        self.ser = None
        self.connected = False
        self.lock = threading.Lock()
        self._stop = False
        self.listeners = []
        self._connect()

    def _find_port(self):
        keywords = ["xiao", "esp32", "ch340", "ch9102", "cp210", "ftdi", "usb serial", "cdc"]
        ports = serial.tools.list_ports.comports()
        for p in ports:
            desc = (p.description or "").lower()
            hw = (p.hwid or "").lower()
            if any(k in desc or k in hw for k in keywords):
                log(f"Found: {p.device}  ({p.description})")
                return p.device
        for p in ports:
            if "usb" in (p.description or "").lower():
                log(f"Trying: {p.device}")
                return p.device
        return None

    def _connect(self):
        port = self.port or self._find_port()
        if not port:
            log("No ESP32 found.")
            return False
        try:
            self.ser = serial.Serial(port, self.baud, timeout=0.1)
            time.sleep(1.5)
            self.connected = True
            log(f"Connected -> {port} @{self.baud} baud")
            self._start_reader()
            return True
        except serial.SerialException as e:
            log(f"Serial error: {e}")
            return False

    def _start_reader(self):
        def read_loop():
            while not self._stop and self.connected:
                try:
                    if self.ser and self.ser.in_waiting > 0:
                        data = self.ser.readline().decode("utf-8", errors="ignore").strip()
                        if data:
                            log(f"  ESP32 <- {data}")
                            for cb in self.listeners:
                                try:
                                    cb(data)
                                except:
                                    pass
                except (serial.SerialException, OSError):
                    log("Serial disconnected.")
                    self.connected = False
                    break
                except:
                    pass
                time.sleep(0.05)
        t = threading.Thread(target=read_loop, daemon=True)
        t.start()

    def send(self, command):
        if not self.connected or not self.ser:
            return False
        try:
            with self.lock:
                self.ser.write((command + "\n").encode("utf-8"))
                self.ser.flush()
            log(f"  SENT -> {command}")
            return True
        except (serial.SerialException, OSError):
            self.connected = False
            return False

    def add_listener(self, cb):
        self.listeners.append(cb)

    def stop(self):
        self._stop = True
        if self.ser:
            try:
                self.ser.close()
            except:
                pass
        self.connected = False


# ── TCP Server ─────────────────────────────────────────────────
class TcpServer:
    def __init__(self, bridge, host="127.0.0.1", port=12345):
        self.bridge = bridge
        self.host = host
        self.port = port
        self.server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server.bind((host, port))
        self.server.listen(5)
        self.server.settimeout(1.0)
        self._stop = False
        log(f"TCP server listening on {host}:{port}")

    def _handle_client(self, conn, addr):
        log(f"Client connected: {addr}")
        # Register a callback for this client to receive ESP32 responses
        def esp_callback(data):
            try:
                conn.sendall(f"ESP32: {data}\n".encode("utf-8"))
            except:
                pass
        self.bridge.add_listener(esp_callback)
        try:
            conn.settimeout(0.5)
            buffer = ""
            while not self._stop:
                try:
                    data = conn.recv(4096).decode("utf-8")
                    if not data:
                        break
                    buffer += data
                    while "\n" in buffer:
                        line, buffer = buffer.split("\n", 1)
                        line = line.strip()
                        if line:
                            self.bridge.send(line)
                except socket.timeout:
                    continue
                except (ConnectionResetError, BrokenPipeError, OSError):
                    break
        finally:
            log(f"Client disconnected: {addr}")
            if esp_callback in self.bridge.listeners:
                self.bridge.listeners.remove(esp_callback)
            try:
                conn.close()
            except:
                pass

    def run(self):
        while not self._stop:
            try:
                conn, addr = self.server.accept()
                t = threading.Thread(target=self._handle_client, args=(conn, addr), daemon=True)
                t.start()
            except socket.timeout:
                continue
            except OSError:
                break

    def stop(self):
        self._stop = True


# ── Main ───────────────────────────────────────────────────────
def main():
    import argparse
    parser = argparse.ArgumentParser(description="Serial Daemon for ESP32")
    parser.add_argument("--port", help="Serial port (e.g. COM3)")
    parser.add_argument("--baud", type=int, default=BAUD, help=f"Baud rate (default: {BAUD})")
    parser.add_argument("--tcp-port", type=int, default=12345, help="TCP port (default: 12345)")
    args = parser.parse_args()

    tcp_port = args.tcp_port
    HOST = "127.0.0.1"

    log("=" * 50)
    log("Serial Daemon starting...")
    log("=" * 50)

    bridge = SerialBridge(port=args.port, baud=args.baud)

    # Keep trying to connect if not immediately available
    if not bridge.connected:
        log("Retrying connection every 5 seconds...")
        def retry():
            while not bridge._stop and not bridge.connected:
                time.sleep(5)
                bridge._connect()
        t = threading.Thread(target=retry, daemon=True)
        t.start()

    server = TcpServer(bridge, host=HOST, port=tcp_port)

    def shutdown(sig=None, frame=None):
        log("\nShutting down...")
        server.stop()
        bridge.stop()
        sys.exit(0)

    signal.signal(signal.SIGINT, shutdown)
    signal.signal(signal.SIGTERM, shutdown)

    try:
        server.run()
    except KeyboardInterrupt:
        shutdown()


if __name__ == "__main__":
    main()
