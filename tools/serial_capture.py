#!/usr/bin/env python3
"""Serial capture — saves to file with millisecond timestamps."""
import sys, os, serial, serial.tools.list_ports, time
from datetime import datetime

def find_port():
    for p in serial.tools.list_ports.comports():
        hwid = ((p.hwid or '') + ' ' + (p.description or '')).lower()
        if any(x in hwid for x in ['xiao', 'esp32', 'ch340', 'cp210', 'ft232', '303a', 'serial']):
            return p.device
    return None

def ts():
    now = datetime.now()
    return now.strftime('%H:%M:%S.') + f'{now.microsecond // 1000:03d}'

def main():
    port = find_port()
    if not port:
        ports = serial.tools.list_ports.comports()
        if len(ports) == 1:
            port = ports[0].device
            print(f"Auto-selected only available port: {port}")
        elif ports:
            print("Available ports:")
            for p in ports:
                hwid = p.hwid or '(no hwid)'
                print(f"  {p.device}: {p.description} ({hwid})")
            port = input("Enter COM port (e.g. COM3): ").strip()
            if not port:
                sys.exit(1)
        else:
            print("No serial ports found")
            sys.exit(1)

    name = input("Filename (e.g. V2.7.3_M8_001): ").strip()
    if not name:
        name = f'V2.7.3_RV_{int(time.time())}'

    path = os.path.join(os.path.dirname(__file__), '..', 'docs', 'CALIBRATION', 'runtime')
    os.makedirs(path, exist_ok=True)
    fname = os.path.join(path, f'{name}.log')
    if os.path.exists(fname):
        yn = input(f"{fname} exists. Append? (y/N): ").strip().lower()
        if yn != 'y':
            sys.exit(1)

    ser = serial.Serial(port, 115200, timeout=1)
    mode = 'ab' if os.path.exists(fname) and yn == 'y' else 'wb'
    print(f"Capturing {port} → {fname}")
    print("Press Ctrl+C to stop.")
    with open(fname, mode) as f:
        try:
            while True:
                line = ser.readline()
                if line:
                    prefix = ts() + ' -> '
                    f.write(prefix.encode() + line)
                    f.flush()
                    sys.stdout.write(prefix + line.decode(errors='replace'))
                    sys.stdout.flush()
        except KeyboardInterrupt:
            print(f"\nSaved {fname}")
        finally:
            ser.close()

if __name__ == '__main__':
    main()
