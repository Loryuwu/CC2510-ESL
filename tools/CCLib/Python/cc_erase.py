"""
CC2510 Chip Erase - clears DEBUG_LOCKED and erases all flash.
After erase, flash can be read and written freely.
WARNING: This permanently destroys the original firmware!
"""
import serial
import time
import sys

port = sys.argv[1] if len(sys.argv) > 1 else "COM28"

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

send(ser, 0xF0)  # PING
print("Proxy OK")

send(ser, 0x01)  # ENTER debug

chip_id = send(ser, 0x03)
print(f"Chip ID: 0x{chip_id:04x}")

# Update instruction table to CC2510
send(ser, 0xF2)  # CMD_INSTR_UPD -> ANS_READY
table = bytes([2, 0x44, 0x4C, 0x24, 0x1D, 0x55, 0x56, 0x57,
               0x68, 0x28, 0x34, 0x5C, 0x14, 0, 0, 0])
ser.write(table)
ser.flush()
r = ser.read(3)
if len(r) < 3 or r[0] != 1:
    print("ERROR: Instruction table update failed"); sys.exit(1)

send(ser, 0x01)  # Re-enter debug

# Check status before erase
status = send(ser, 0x04) & 0xFF
print(f"Status before erase: 0x{status:02x}")
locked = bool(status & 0x04)
print(f"DEBUG_LOCKED: {'YES' if locked else 'no'}")

if not locked:
    print("Chip is not locked - no erase needed for reading.")
    r = input("Erase anyway? (y/N): ").strip().lower()
    if r != 'y':
        print("Aborted."); sys.exit(0)
else:
    print("\n*** WARNING: Chip Erase will permanently destroy all flash content! ***")
    r = input("Type 'ERASE' to confirm: ").strip()
    if r != 'ERASE':
        print("Aborted."); sys.exit(0)

# Re-enter debug mode for chip erase
send(ser, 0x01)

# CMD_CHPERASE (0x0D)
print("Erasing chip...")
send(ser, 0x0D)

# Poll status until CHIP_ERASE_BUSY (bit 7) clears
for _ in range(200):
    time.sleep(0.05)
    s = send(ser, 0x04) & 0xFF
    if not (s & 0x80):
        break

# Re-enter debug after erase
send(ser, 0x01)

status = send(ser, 0x04) & 0xFF
print(f"Status after erase: 0x{status:02x}")
print(f"DEBUG_LOCKED: {'YES' if status & 0x04 else 'no (UNLOCKED!)'}")

if not (status & 0x04):
    print("\nChip erase successful! Flash is now unlocked.")
    print(f"You can now read flash with: python cc_read_direct.py {port}")
else:
    print("\nWARNING: DEBUG_LOCKED is still set. Erase may have failed.")

ser.close()
