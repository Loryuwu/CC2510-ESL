"""
Direct CC2510 flash reader - bypasses CCLib entirely.
Uses same serial protocol as the working debug test.
"""
import serial
import time
import sys

port = sys.argv[1] if len(sys.argv) > 1 else "COM28"
outfile = sys.argv[2] if len(sys.argv) > 2 else "backup.bin"

def send(ser, cmd, c1=0, c2=0, c3=0):
    ser.write(bytes([cmd, c1, c2, c3]))
    ser.flush()
    r = ser.read(3)
    if len(r) < 3:
        raise IOError(f"Timeout! Sent: {cmd:02x} {c1:02x} {c2:02x} {c3:02x}")
    if r[0] == 0x02:
        raise IOError(f"Error from proxy: 0x{r[2]:02x}")
    return (r[1] << 8) | r[2]

print(f"Connecting to {port}...")
ser = serial.Serial(port, baudrate=115200, timeout=10.0, write_timeout=10.0)
time.sleep(3)

# Ping
send(ser, 0xF0)
print("Proxy OK")

# Enter debug
send(ser, 0x01)

# Get chip ID
chip_id = send(ser, 0x03)
print(f"Chip ID: 0x{chip_id:04x}")

if chip_id == 0x0000:
    print("ERROR: No chip found"); sys.exit(1)

# Update instruction table to CC2510
send(ser, 0xF2)  # CMD_INSTR_UPD -> ANS_READY
table = bytes([2, 0x44, 0x4C, 0x24, 0x1D, 0x55, 0x56, 0x57,
               0x68, 0x28, 0x34, 0x5C, 0x14, 0, 0, 0])
ser.write(table)
ser.flush()
r = ser.read(3)
if len(r) < 3 or r[0] != 1:
    print("ERROR: Instruction table update failed"); sys.exit(1)
print(f"Instruction table updated to CC2510 (v{r[2]})")

# Re-enter debug mode
send(ser, 0x01)
print("Debug mode re-entered")

# Check status
status = send(ser, 0x04) & 0xFF
print(f"Status: 0x{status:02x}", end="")
if status & 0x04:
    print(" [DEBUG_LOCKED - flash content will be 0x00/0xFF]", end="")
print()

# CC2510: 16KB flash, 1KB pages
flash_size = 16 * 1024
page_size = 0x400
flash_data = bytearray()

print(f"\nReading {flash_size // 1024} KB flash to {outfile}...")

for bank in range(flash_size // 0x8000 + 1):
    # selectXDATABank
    send(ser, 0x09, 0x75, 0xC7, bank * 16 + 1)

    bank_start = bank * 0x8000
    bank_end = min(bank_start + 0x8000, flash_size)
    bank_offset = 0

    # Set DPTR to start of bank
    send(ser, 0x09, 0x90, (bank_offset >> 8) & 0xFF, bank_offset & 0xFF)

    for addr in range(bank_start, bank_end):
        # CLR A
        send(ser, 0x07, 0xE4)
        # MOVC A, @A+DPTR
        val = send(ser, 0x07, 0x93) & 0xFF
        flash_data.append(val)
        # INC DPTR
        send(ser, 0x07, 0xA3)

        if (addr + 1) % 256 == 0:
            pct = (addr + 1) * 100 // flash_size
            print(f"\r  Progress: {pct}% ({addr + 1}/{flash_size} bytes)", end="")
            sys.stdout.flush()

print(f"\r  Progress: 100% ({flash_size}/{flash_size} bytes)")

# Save
with open(outfile, "wb") as f:
    f.write(flash_data)

print(f"\nSaved to {outfile} ({len(flash_data)} bytes)")

# Check if it's all zeros (DEBUG_LOCKED)
if all(b == 0 for b in flash_data):
    print("\nWARNING: All bytes are 0x00 - chip is DEBUG_LOCKED!")
    print("Use chip erase to unlock (erases all flash):")
    print(f"  python cc_erase_direct.py {port}")

ser.close()
