#!/usr/bin/env python3
"""Capture UART from COM port into debug-49b1f2.log as NDJSON"""
import json, sys, time
from pathlib import Path

try:
    import serial
except ImportError:
    sys.exit("pyserial missing")

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM5"
BAUD = int(sys.argv[2]) if len(sys.argv) > 2 else 115200
SECS = float(sys.argv[3]) if len(sys.argv) > 3 else 20.0
LOG = Path("debug-49b1f2.log")

def main():
    print(f"Opening {PORT} {BAUD}, {SECS}s -> {LOG}")
    ser = serial.Serial(PORT, BAUD, timeout=0.2)
    ser.reset_input_buffer()
    t_end = time.time() + SECS
    n = 0
    buf = b""
    with LOG.open("a", encoding="utf-8") as f:
        while time.time() < t_end:
            chunk = ser.read(256)
            if not chunk:
                continue
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                text = line.decode("utf-8", errors="replace").strip()
                if not text:
                    continue
                n += 1
                entry = {
                    "sessionId": "49b1f2",
                    "runId": "uart-capture",
                    "hypothesisId": "P,Q,R",
                    "location": "COM_UART",
                    "message": text,
                    "data": {"line": text},
                    "timestamp": int(time.time() * 1000),
                }
                f.write(json.dumps(entry, ensure_ascii=False) + "\n")
                print(text)
    ser.close()
    print(f"Done, {n} lines")

if __name__ == "__main__":
    main()
