"""
CC2510 flash writer with auto page-erase.
Writes firmware by loading data + flash write routine into SRAM,
mapping SRAM to CODE space (MEMCTR=0x51), and executing via RESUME.

Robust against the proxy dropping the odd serial byte: a comms
glitch resyncs the proxy and retries just the affected chunk/page instead of
aborting the whole flash. A final verify triggers up to a few full re-flashes
if anything is still wrong.
"""
import serial
import time
import sys

port = sys.argv[1] if len(sys.argv) > 1 else "COM28"
infile = sys.argv[2] if len(sys.argv) > 2 else None

if not infile:
    print("Usage: python cc_write_direct.py <port> <firmware.bin>")
    sys.exit(1)

with open(infile, "rb") as f:
    firmware = f.read()

while len(firmware) % 2 != 0:
    firmware += b'\xFF'

print(f"Firmware: {len(firmware)} bytes")

PAGE_SIZE = 1024
CHUNK = 128  # bytes per write (fits in XDATA 0xF000-0xF07F, routine at 0xF100)

CHUNK_RETRIES = 10   # per-chunk comms retries before giving up
FLASH_RETRIES = 3    # whole-flash attempts if final verify fails

FLASH_WRITE_ROUTINE = bytearray([
    0x75, 0xAD, 0x00,  # MOV FADDRH, #imm  [byte 2 = addrH]
    0x75, 0xAC, 0x00,  # MOV FADDRL, #imm  [byte 5 = addrL]
    0x90, 0xF0, 0x00,  # MOV DPTR, #0xF000
    0x75, 0xAE, 0x02,  # MOV FCTL, #0x02 (WRITE)
    0x7D, 0x08,         # MOV R5, #imm      [byte 13 = word count]
    0xE0,               # MOVX A, @DPTR
    0xF5, 0xAF,         # MOV FWDATA, A
    0xA3,               # INC DPTR
    0xE0,               # MOVX A, @DPTR
    0xF5, 0xAF,         # MOV FWDATA, A
    0xA3,               # INC DPTR
    0xE5, 0xAE,         # MOV A, FCTL
    0x20, 0xE6, 0xFB,   # JB ACC.6, writeWaitLoop (SWBSY)
    0xDD, 0xF1,         # DJNZ R5, writeWordLoop
    0xA5,               # halt
])


class CommsError(IOError):
    pass


def send(ser, cmd, c1=0, c2=0, c3=0):
    ser.write(bytes([cmd, c1, c2, c3]))
    ser.flush()
    r = ser.read(3)
    if len(r) < 3:
        raise CommsError(f"Timeout! cmd=0x{cmd:02x}")
    if r[0] == 0x02:
        raise CommsError(f"Error: 0x{r[2]:02x}")
    return (r[1] << 8) | r[2]


def exec1(ser, oc):
    return send(ser, 0x07, oc)

def exec2(ser, oc0, oc1):
    return send(ser, 0x08, oc0, oc1)

def exec3(ser, oc0, oc1, oc2):
    return send(ser, 0x09, oc0, oc1, oc2)


def write_xdata(ser, addr, data):
    exec3(ser, 0x90, (addr >> 8) & 0xFF, addr & 0xFF)
    for b in data:
        exec2(ser, 0x74, b)
        exec1(ser, 0xF0)
        exec1(ser, 0xA3)


def resync(ser):
    """Recover the proxy after a dropped byte: flush, re-enter debug."""
    try:
        ser.reset_input_buffer()
        ser.reset_output_buffer()
    except Exception:
        pass
    time.sleep(0.15)
    for _ in range(4):
        try:
            send(ser, 0x01)              # ENTER debug (also halts)
            cid = send(ser, 0x03)        # chip id
            if cid != 0x0000:
                return True
        except CommsError:
            try:
                ser.reset_input_buffer()
            except Exception:
                pass
            time.sleep(0.2)
    return False


def page_erase(ser, page_byte_addr):
    """Erase one 1KB flash page via DEBUG_INSTR (like imagotag cc_pageErase)."""
    adr_h = ((page_byte_addr >> 8) // 2) & 0x7E
    exec3(ser, 0x75, 0xAD, adr_h)   # MOV FADDRH, #adrH
    exec3(ser, 0x75, 0xAC, 0x00)    # MOV FADDRL, #0
    exec3(ser, 0x75, 0xAE, 0x01)    # MOV FCTL, #0x01 (ERASE)
    for _ in range(500):
        time.sleep(0.005)
        fctl = exec2(ser, 0xE5, 0xAE) & 0xFF  # MOV A, FCTL
        if not (fctl & 0x80):  # BUSY cleared
            return True
    print(f"  WARNING: Page erase timeout at 0x{page_byte_addr:04X}")
    return False


def write_flash_chunk(ser, flash_byte_addr, data):
    while len(data) % 2 != 0:
        data += b'\xFF'

    word_addr = flash_byte_addr // 2
    word_count = len(data) // 2

    routine = bytearray(FLASH_WRITE_ROUTINE)
    routine[2] = (word_addr >> 8) & 0xFF
    routine[5] = word_addr & 0xFF
    routine[13] = word_count

    write_xdata(ser, 0xF000, data)
    write_xdata(ser, 0xF100, routine)

    exec3(ser, 0x75, 0xC7, 0x51)  # MEMCTR = 0x51
    exec3(ser, 0x02, 0xF1, 0x00)  # LJMP 0xF100
    send(ser, 0x0E)               # RESUME

    for _ in range(200):
        time.sleep(0.01)
        s = send(ser, 0x04) & 0xFF
        if s & 0x08:
            return True

    # CPU didn't report halt - re-enter debug and let the caller retry/verify.
    send(ser, 0x01)
    raise CommsError(f"CPU halt timeout at 0x{flash_byte_addr:04X}")


# ---- robust wrappers ----

def erase_all(ser, num_pages):
    print(f"\nErasing {num_pages} page(s)...")
    for p in range(num_pages):
        addr = p * PAGE_SIZE
        print(f"  Page {p} (0x{addr:04X}-0x{addr+PAGE_SIZE-1:04X})...", end="")
        ok = False
        for attempt in range(CHUNK_RETRIES):
            try:
                send(ser, 0x01)            # ensure halted
                ok = page_erase(ser, addr)
                if ok:
                    break
            except CommsError:
                if not resync(ser):
                    time.sleep(0.3)
        print(" OK" if ok else " FAILED")
        if not ok:
            return False
    return True


def write_all(ser, firmware):
    total = (len(firmware) + CHUNK - 1) // CHUNK
    print(f"\nWriting {len(firmware)} bytes in {total} chunks of {CHUNK}...")
    glitches = 0
    for ci in range(total):
        offset = ci * CHUNK
        chunk = firmware[offset:offset + CHUNK]
        while len(chunk) < CHUNK:
            chunk += b'\xFF'

        ok = False
        for attempt in range(CHUNK_RETRIES):
            try:
                send(ser, 0x01)            # enter debug / halt
                write_flash_chunk(ser, offset, bytes(chunk))
                ok = True
                break
            except CommsError:
                glitches += 1
                resync(ser)                # recover and redo this chunk
        if not ok:
            print(f"\n  Chunk {ci} (0x{offset:04X}) failed after {CHUNK_RETRIES} retries")
            return False

        pct = (ci + 1) * 100 // total
        written = min((ci + 1) * CHUNK, len(firmware))
        print(f"\r  Progress: {pct}% ({written}/{len(firmware)} bytes)"
              f"{'  [glitches recovered: %d]' % glitches if glitches else ''}", end="")
        sys.stdout.flush()
    print()
    return True


def verify(ser, firmware):
    print("--- Verify ---")
    for attempt in range(CHUNK_RETRIES):
        try:
            send(ser, 0x01)
            exec3(ser, 0x75, 0xC7, 0x01)   # MEMCTR = 0x01 (flash in CODE)
            exec3(ser, 0x90, 0x00, 0x00)   # DPTR = 0x0000
            errors = 0
            first_bad = []
            for i in range(len(firmware)):
                exec1(ser, 0xE4)           # CLR A
                val = exec1(ser, 0x93) & 0xFF  # MOVC A,@A+DPTR
                exec1(ser, 0xA3)           # INC DPTR
                if val != firmware[i]:
                    errors += 1
                    if len(first_bad) < 10:
                        first_bad.append((i, firmware[i], val))
                if (i + 1) % 64 == 0:
                    pct = (i + 1) * 100 // len(firmware)
                    print(f"\r  Verifying: {pct}%", end="")
                    sys.stdout.flush()
            print()
            for (i, w, r) in first_bad:
                print(f"  [{i:04X}] wrote:0x{w:02X} read:0x{r:02X}")
            print(f"  Result: {errors} errors / {len(firmware)} bytes")
            return errors
        except CommsError:
            print("\n  (verify comms glitch, resyncing...)")
            resync(ser)
    return -1


# ---- Connect ----
print(f"Connecting to {port}...")
ser = serial.Serial(port, baudrate=115200, timeout=2.0, write_timeout=10.0)
time.sleep(3)

send(ser, 0xF0)  # PING
print("Proxy OK")

# Try entering debug up to 3 times
chip_id = 0
for attempt in range(3):
    send(ser, 0x01)  # ENTER debug
    time.sleep(0.1)
    chip_id = send(ser, 0x03)
    if chip_id != 0x0000:
        break
    print(f"  Attempt {attempt+1}: Chip ID 0x{chip_id:04x}, retrying...")
    time.sleep(0.5)

print(f"Chip ID: 0x{chip_id:04x}")
if chip_id == 0x0000:
    print("ERROR: Cannot communicate with CC2510!")
    print("  - Check RST wire (ESP32 GPIO6 -> CC2510 RST)")
    print("  - Check DD wire  (ESP32 GPIO5 -> CC2510 P2.1)")
    print("  - Check DC wire  (ESP32 GPIO4 -> CC2510 P2.2)")
    print("  - Try unplugging/replugging the ESP32")
    ser.close()
    sys.exit(1)

status = send(ser, 0x04) & 0xFF
print(f"Status: 0x{status:02x} (halted={bool(status & 0x08)})")

send(ser, 0xF2)
table = bytes([2, 0x44, 0x4C, 0x24, 0x1D, 0x55, 0x56, 0x57,
               0x68, 0x28, 0x34, 0x5C, 0x14, 0, 0, 0])
ser.write(table)
ser.flush()
r = ser.read(3)
if len(r) < 3 or r[0] != 1:
    print("ERROR: Table update failed"); sys.exit(1)

send(ser, 0x01)

# ---- Flash with full-reflash retry ----
num_pages = (len(firmware) + PAGE_SIZE - 1) // PAGE_SIZE
errors = -1
for flash_attempt in range(FLASH_RETRIES):
    if flash_attempt:
        print(f"\n=== Re-flash attempt {flash_attempt+1}/{FLASH_RETRIES} ===")
        resync(ser)

    if not erase_all(ser, num_pages):
        continue
    if not write_all(ser, firmware):
        continue
    errors = verify(ser, firmware)
    if errors == 0:
        break

if errors == 0:
    print("\nFlash write SUCCESS!")
    exec3(ser, 0x75, 0xC7, 0x00)
    exec3(ser, 0x02, 0x00, 0x00)
    send(ser, 0x0E)
    print("Chip running from 0x0000.")
    print("\nDisconnect debug wires and power tag standalone to test.")
else:
    print("\nFlash write FAILED after retries.")

ser.close()
