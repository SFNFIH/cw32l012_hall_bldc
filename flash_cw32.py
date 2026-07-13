#!/usr/bin/env python3
"""CW32L012 flash programmer using pyOCD — follows User Manual V1.2 Ch.7"""
import sys, time
from pyocd.core.helpers import ConnectHelper
from elftools.elf.elffile import ELFFile

# DAPLink: 留空自动检测，指定ID则只使用该探针
PROBE_ID = ""   # 例如: "8716E9B021D2E4E10D3F4C73D01DD0E8"
PAGE_SZ  = 512

# Register addresses (FLASH_BASE = 0x40022000)
FLASH_CR1  = 0x40022000  # Control register 1 (MODE, KEY)
FLASH_CR2  = 0x40022004  # Control register 2 (WAIT, FETCH, CACHE, KEY)
FLASH_PLOCK= 0x40022008  # Page lock register (LOCKx, KEY)
FLASH_ISR  = 0x40022024  # Interrupt status register (BUSY, PROG, CACHEON, SDKERR, PAGELOCK, PC)
AHBEN      = 0x40004030  # AHB enable register (SYSCTRL)

def flash_error(stat):
    """Decode FLASH_ISR error flags"""
    errs = []
    if stat & 0x10: errs.append("PROG")
    if stat & 0x08: errs.append("CACHEON")
    if stat & 0x04: errs.append("SDKERR")
    if stat & 0x02: errs.append("PAGELOCK")
    if stat & 0x01: errs.append("PC")
    return errs

def check_errors(t, step):
    """Check FLASH_ISR for errors after an operation"""
    isr = t.read32(FLASH_ISR)
    errs = flash_error(isr)
    if errs:
        print(f"  !! FLASH ERROR at '{step}': {errs} (ISR=0x{isr:08X})")
        # Clear error flags by writing 0 to ICR bits
        t.write32(0x40022028, 0x5A5A0000)
        return False
    return True

def main():
    elf = sys.argv[1] if len(sys.argv) > 1 else "build/Debug/cw32l012.elf"
    session = ConnectHelper.session_with_chosen_probe(unique_id=PROBE_ID, connect_mode="under-reset") if PROBE_ID else ConnectHelper.session_with_chosen_probe(connect_mode="under-reset")
    if not session: sys.exit("Cannot connect. Check probe.")
    with session:
        t = session.target
        print(f"Connected: {t.cores[0].name}")

        # ---- Reset CPU and move PC to RAM to avoid PC-page protection ----
        t.reset_and_halt()
        t.write_core_register('pc', 0x20000000)
        print(f"  CPU halted, PC moved to RAM")

        # ---- Step 1: Enable FLASH clock (AHBEN bit1) ----
        ahben = t.read32(AHBEN)
        t.write32(AHBEN, 0x5A5A0000 | (ahben & 0xFFFF) | 0x02)
        print(f"  FLASH clock enabled (AHBEN=0x{t.read32(AHBEN) & 0xFFFF:04X})")

        # ---- Step 2: Disable CACHE and FETCH (CR2) ----
        cr2 = t.read32(FLASH_CR2)
        t.write32(FLASH_CR2, 0x5A5A0000 | (cr2 & ~0x18))  # clear CACHE(bit4) + FETCH(bit3)
        print(f"  CR2: CACHE=0 FETCH=0")

        # ---- Step 3: Unlock ALL pages (LOCKx=1 = unlock per manual §7.6.1) ----
        t.write32(FLASH_PLOCK, 0x5A5AFFFF)
        plock = t.read32(FLASH_PLOCK) & 0xFFFF
        print(f"  PAGELOCK=0x{plock:04X} {'OK' if plock == 0xFFFF else 'WARN'}")

        # ---- Read ELF segments ----
        elffile = ELFFile(open(elf, 'rb'))
        ok = True
        erased_pages = set()  # 追踪已擦除的页, 防止重复擦除覆盖已写入数据
        for seg in elffile.iter_segments():
            if seg.header.p_type != 'PT_LOAD':
                continue
            # 使用物理地址 (LMA) 判断目标位置
            # .data 段 VADDR 在 RAM 但 LMA 在 FLASH
            addr = seg.header.p_paddr
            data = seg.data()
            if addr >= 0x20000000 or len(data) == 0:  # 跳过纯RAM段和空段(.bss)
                continue
            print(f"  FLASH: 0x{addr:08X} size={len(data)}")

            # ---- Step 4: Page erase (MODE=2) ----
            start_page = addr // PAGE_SZ
            end_page   = (addr + len(data) + PAGE_SZ - 1) // PAGE_SZ
            for p in range(start_page, end_page):
                if p in erased_pages:
                    continue  # 已擦除, 跳过 (避免擦除前面段已写入的数据)
                erased_pages.add(p)
                # Set MODE=2 (page erase)
                cr1 = t.read32(FLASH_CR1)
                t.write32(FLASH_CR1, 0x5A5A0000 | (cr1 & ~3) | 0x02)
                # Trigger erase: write any byte to page
                t.write8(p * PAGE_SZ, 0x00)
                # Wait BUSY
                while t.read32(FLASH_ISR) & 0x20:
                    pass
                if not check_errors(t, f"erase page {p}"):
                    ok = False
                    break
            if not ok:
                break

            # ---- Step 5: Word program (MODE=1) ----
            for i in range(0, len(data), 4):
                chunk = data[i:i+4]
                if len(chunk) < 4:
                    chunk = chunk + b'\xFF' * (4 - len(chunk))
                word = int.from_bytes(chunk, 'little')
                # Set MODE=1 (program)
                cr1 = t.read32(FLASH_CR1)
                t.write32(FLASH_CR1, 0x5A5A0000 | (cr1 & ~3) | 0x01)
                # Write 32-bit word
                t.write32(addr + i, word)
                # Wait BUSY
                while t.read32(FLASH_ISR) & 0x20:
                    pass
                if not check_errors(t, f"prog 0x{addr+i:08X}"):
                    ok = False
                    break
            if not ok:
                break

        # ---- Step 6: Restore ----
        # Set MODE=0 (read mode)
        t.write32(FLASH_CR1, 0x5A5A0000)

        # Re-lock pages (LOCKx=0 = lock)
        t.write32(FLASH_PLOCK, 0x5A5A0000)

        # Re-enable CACHE + FETCH in CR2 (restore original or enable by default)
        t.write32(FLASH_CR2, 0x5A5A0000 | 0x18)  # CACHE=1, FETCH=1

        # ---- Verify ----
        if ok:
            # Verify vector table
            sp = t.read32(0x00000000)
            pc = t.read32(0x00000004)
            print(f"  Verify SP=0x{sp:08X} {'OK' if sp == 0x20002000 else 'FAIL'}")
            print(f"  Verify PC=0x{pc:08X} (Reset_Handler)")

        if ok:
            t.reset_and_halt()
            print("DONE. Ready to debug.")
        else:
            print("FLASH PROGRAMMING FAILED!")

if __name__ == '__main__':
    main()
