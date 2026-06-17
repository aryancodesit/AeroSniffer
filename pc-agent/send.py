#!/usr/bin/env python3
"""
Send commands to the ESP32 via the serial daemon.

Usage:
    python send.py "FACE:HAPPY"
    python send.py "STATUS:Hello World" "APP:VSCODE"
    python send.py --listen                  # Listen for ESP32 responses
    echo "FACE:HAPPY" | python send.py       # Pipe from stdin
    python send.py --json '{"emotion":"happy","activity":"coding","app":"VSCODE"}'
"""

import socket
import sys
import json

HOST = "127.0.0.1"
PORT = 12345


def send_command(cmd):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5.0)
        s.connect((HOST, PORT))
        s.sendall((cmd + "\n").encode("utf-8"))
        # Read any immediate response
        try:
            while True:
                data = s.recv(4096).decode("utf-8")
                if not data:
                    break
                print(data, end="", flush=True)
        except socket.timeout:
            pass
        s.close()
        return True
    except ConnectionRefusedError:
        print("[ERROR] Daemon not running. Start it with: python serial_daemon.py")
        return False
    except Exception as e:
        print(f"[ERROR] {e}")
        return False


def listen():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(None)
        s.connect((HOST, PORT))
        print("Listening for ESP32 responses... (Ctrl+C to stop)")
        while True:
            data = s.recv(4096).decode("utf-8")
            if not data:
                break
            print(data, end="", flush=True)
    except ConnectionRefusedError:
        print("[ERROR] Daemon not running. Start it with: python serial_daemon.py")
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        s.close()


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    if sys.argv[1] == "--listen":
        listen()
        return

    # Read commands from args or stdin
    if len(sys.argv) > 1 and sys.argv[1] == "--json":
        cmd = json.dumps(sys.argv[2]) if len(sys.argv) > 2 else sys.stdin.read().strip()
        send_command(cmd)
    elif len(sys.argv) > 1:
        for cmd in sys.argv[1:]:
            send_command(cmd)
            if len(sys.argv) > 2:
                import time
                time.sleep(0.2)  # Brief pause between multiple commands
    else:
        for line in sys.stdin:
            cmd = line.strip()
            if cmd:
                send_command(cmd)


if __name__ == "__main__":
    main()
