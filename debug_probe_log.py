#!/usr/bin/env python3
"""Poll MCU debug mailbox via SWD and append NDJSON to debug-49b1f2.log"""
import json, struct, sys, time
from pathlib import Path
from pyocd.core.helpers import ConnectHelper
from elftools.elf.elffile import ELFFile

ELF = Path("build/Debug/cw32l012_hall_bldc.elf")
LOG = Path("debug-49b1f2.log")
SESSION = "49b1f2"
MAGIC = 0x49B1DB60

def find_symbol(elf_path, name):
    with open(elf_path, "rb") as f:
        elf = ELFFile(f)
        symtab = elf.get_section_by_name(".symtab")
        if symtab is None:
            return None
        for s in symtab.iter_symbols():
            if s.name == name:
                return s["st_value"], s["st_size"]
    return None

def main():
    duration = float(sys.argv[1]) if len(sys.argv) > 1 else 25.0
    sym = find_symbol(ELF, "g_dbg_mbox")
    if not sym:
        sys.exit("g_dbg_mbox not found")
    addr, size = sym
    print(f"g_dbg_mbox @ 0x{addr:08X} size={size}, {duration}s")

    session = ConnectHelper.session_with_chosen_probe(connect_mode="attach")
    if not session:
        sys.exit("Cannot connect SWD")

    last_stamp = None
    n = 0
    t_end = time.time() + duration
    with session:
        t = session.target
        while time.time() < t_end:
            raw = bytes(t.read_memory_block8(addr, min(size, 48)))
            magic = struct.unpack_from("<I", raw, 0)[0]
            if magic != MAGIC:
                time.sleep(0.05)
                continue
            motor_on = raw[4]
            hall = raw[5]
            duty = struct.unpack_from("<H", raw, 6)[0]
            adc = struct.unpack_from("<H", raw, 8)[0]
            skip = struct.unpack_from("<H", raw, 10)[0]
            step_dt = struct.unpack_from("<I", raw, 12)[0]
            moe = raw[16]
            direction = raw[17]
            flip = struct.unpack_from("<H", raw, 18)[0]
            rej = struct.unpack_from("<H", raw, 20)[0]
            fw = struct.unpack_from("<H", raw, 22)[0]
            bw = struct.unpack_from("<H", raw, 24)[0]
            irq = struct.unpack_from("<H", raw, 26)[0]
            ok = struct.unpack_from("<H", raw, 28)[0]
            stamp = struct.unpack_from("<I", raw, 32)[0]  # aligned after ok
            if stamp == last_stamp:
                time.sleep(0.05)
                continue
            last_stamp = stamp
            n += 1
            data = {
                "motor_on": motor_on, "hall": hall, "duty": duty, "adc": adc,
                "skip": skip, "step_dt": step_dt, "moe": moe, "dir": direction,
                "flip": flip, "blank": rej, "fw": fw, "bw": bw,
                "irq": irq, "ok": ok,
            }
            entry = {
                "sessionId": SESSION, "runId": "no-comm-repro",
                "hypothesisId": "H1,H2,H3", "location": "g_dbg_mbox",
                "message": "hall_comm_stats", "data": data,
                "timestamp": int(time.time() * 1000),
            }
            with LOG.open("a", encoding="utf-8") as f:
                f.write(json.dumps(entry) + "\n")
            print(
                f"[{n}] on={motor_on} hall={hall} duty={duty} "
                f"fw={fw} bw={bw} flip={flip} ok={ok} irq={irq} blank={rej}"
            )
            time.sleep(0.08)
    print(f"Done. {n} samples -> {LOG}")

if __name__ == "__main__":
    main()
