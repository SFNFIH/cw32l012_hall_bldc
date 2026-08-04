#!/usr/bin/env python3
"""One-shot ATIM/hall/mailbox dump via SWD"""
import json, struct, time
from pathlib import Path
from pyocd.core.helpers import ConnectHelper

ATIM = 0x40001400
HALL_STATE = 0x40006420
LOG = Path("debug-49b1f2.log")
MBOX = 0x2000008C  # may change after rebuild; prefer symbol

def main():
    from elftools.elf.elffile import ELFFile
    elf = Path("build/Debug/cw32l012_pwm_dma.elf")
    addr = MBOX
    with open(elf, "rb") as f:
        e = ELFFile(f)
        sym = e.get_section_by_name(".symtab")
        for s in sym.iter_symbols():
            if s.name == "g_dbg_mbox":
                addr = s["st_value"]
                break

    with ConnectHelper.session_with_chosen_probe(connect_mode="halt") as session:
        t = session.target
        ccer = t.read32(ATIM + 0x20)
        bdtr = t.read32(ATIM + 0x44)
        ccmr1 = t.read32(ATIM + 0x18)
        ccmr2 = t.read32(ATIM + 0x1C)
        ccr1 = t.read32(ATIM + 0x34) & 0xFFFF
        ccr2 = t.read32(ATIM + 0x38) & 0xFFFF
        ccr3 = t.read32(ATIM + 0x3C) & 0xFFFF
        arr = t.read32(ATIM + 0x2C) & 0xFFFF
        hall_st = t.read32(HALL_STATE)
        raw = bytes(t.read_memory_block8(addr, 36))
        magic = struct.unpack_from("<I", raw, 0)[0]
        data = {
            "ccer": f"0x{ccer:08X}",
            "CC1E": (ccer >> 0) & 1,
            "CC1NE": (ccer >> 2) & 1,
            "CC2E": (ccer >> 4) & 1,
            "CC2NE": (ccer >> 6) & 1,
            "CC3E": (ccer >> 8) & 1,
            "CC3NE": (ccer >> 10) & 1,
            "moe": (bdtr >> 15) & 1,
            "ossi": (bdtr >> 10) & 1,
            "ossr": (bdtr >> 11) & 1,
            "bdtr": f"0x{bdtr:08X}",
            "ccmr1": f"0x{ccmr1:08X}",
            "ccmr2": f"0x{ccmr2:08X}",
            "ccr1": ccr1, "ccr2": ccr2, "ccr3": ccr3, "arr": arr,
            "hall_state": f"0x{hall_st:02X}",
            "hall_f": hall_st & 7,
            "mbox_magic": f"0x{magic:08X}",
            "mbox_on": raw[4] if magic == 0x49B1DB60 else None,
            "mbox_hall": raw[5] if magic == 0x49B1DB60 else None,
            "mbox_duty": struct.unpack_from("<H", raw, 6)[0] if magic == 0x49B1DB60 else None,
            "mbox_moe": raw[16] if magic == 0x49B1DB60 else None,
            "mbox_ccer": f"0x{struct.unpack_from('<I', raw, 20)[0]:08X}" if magic == 0x49B1DB60 else None,
            "mbox_stamp": struct.unpack_from("<I", raw, 28)[0] if magic == 0x49B1DB60 else None,
        }
        n_en = data["CC1E"] + data["CC1NE"] + data["CC2E"] + data["CC2NE"] + data["CC3E"] + data["CC3NE"]
        data["enable_count"] = n_en
        data["all_phases_suspect"] = n_en >= 5  # 3ph full = 6, normal 6-step = 3 enables
        entry = {
            "sessionId": "49b1f2",
            "runId": "pre-fix",
            "hypothesisId": "F,G,H,I",
            "location": "dump_atim.py",
            "message": "atim_snapshot",
            "data": data,
            "timestamp": int(time.time() * 1000),
        }
        print(json.dumps(data, indent=2))
        with LOG.open("a", encoding="utf-8") as f:
            f.write(json.dumps(entry) + "\n")
        print(f"wrote {LOG}")

if __name__ == "__main__":
    main()
