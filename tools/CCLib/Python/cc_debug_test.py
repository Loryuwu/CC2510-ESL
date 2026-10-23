"""
Step-by-step debug test for CC2510 via CCLib proxy.
Shows every byte sent/received.
"""
import serial
import time
import sys

port = sys.argv[1] if len(sys.argv) > 1 else "COM28"

CMD_ENTER     = 0x01
CMD_CHIP_ID   = 0x03
CMD_STATUS    = 0x04
CMD_EXEC_1    = 0x07
CMD_EXEC_2    = 0x08
CMD_EXEC_3    = 0x09
CMD_RD_CFG    = 0x0B
CMD_PING      = 0xF0
CMD_INSTR_VER = 0xF1
CMD_INSTR_UPD = 0xF2

def send_cmd(ser, name, cmd, c1=0, c2=0, c3=0):
    data = bytes([cmd, c1, c2, c3])
    print(f"  TX: {data.hex(' ')} ({name})")
    ser.write(data)
    ser.flush()
    resp = ser.read(3)
    if len(resp) < 3:
        print(f"  RX: TIMEOUT ({len(resp)} bytes)")
        return None
    print(f"  RX: {resp.hex(' ')} (status={resp[0]} val=0x{resp[1]:02x}{resp[2]:02x})")
    return resp

print(f"Opening {port}...")
ser = serial.Serial(port, baudrate=115200, timeout=5.0, write_timeout=5.0)
time.sleep(3)
print()

# Step 1: Ping
print("[1] PING")
r = send_cmd(ser, "PING", CMD_PING)
if r is None or r[0] != 1:
    print("FAIL: Proxy not responding"); sys.exit(1)
print("OK\n")

# Step 2: Enter debug
print("[2] ENTER")
r = send_cmd(ser, "ENTER", CMD_ENTER)
print()

# Step 3: Chip ID
print("[3] CHIP_ID")
r = send_cmd(ser, "CHIP_ID", CMD_CHIP_ID)
if r and r[0] == 1:
    chip_id = (r[1] << 8) | r[2]
    print(f"  Chip ID: 0x{chip_id:04x}")
print()

# Step 4: Status (with CC254x table - may return wrong data)
print("[4] STATUS (CC254x table, may be wrong)")
r = send_cmd(ser, "STATUS", CMD_STATUS)
print()

# Step 5: Config (with CC254x table)
print("[5] RD_CONFIG (CC254x table, may be wrong)")
r = send_cmd(ser, "RD_CFG", CMD_RD_CFG)
print()

# Step 6: Get instruction table version
print("[6] INSTR_VER")
r = send_cmd(ser, "INSTR_VER", CMD_INSTR_VER)
print()

# Step 7: Update instruction table to CC2510
print("[7] INSTR_UPD (CC2510 table)")
data = bytes([CMD_INSTR_UPD, 0, 0, 0])
print(f"  TX: {data.hex(' ')} (INSTR_UPD)")
ser.write(data)
ser.flush()
resp = ser.read(3)
if len(resp) < 3:
    print(f"  RX: TIMEOUT"); sys.exit(1)
print(f"  RX: {resp.hex(' ')} (should be ANS_READY=03)")

if resp[0] == 0x03:
    # Send CC2510 instruction table
    table = bytes([
        2,    # version
        0x44, # HALT
        0x4C, # RESUME
        0x24, # RD_CONFIG
        0x1D, # WR_CONFIG
        0x55, # DEBUG_INSTR_1
        0x56, # DEBUG_INSTR_2
        0x57, # DEBUG_INSTR_3
        0x68, # GET_CHIP_ID
        0x28, # GET_PC
        0x34, # READ_STATUS
        0x5C, # STEP_INSTR
        0x14, # CHIP_ERASE
        0, 0, 0
    ])
    print(f"  TX: {table.hex(' ')} (16 bytes table)")
    ser.write(table)
    ser.flush()
    resp = ser.read(3)
    if len(resp) < 3:
        print(f"  RX: TIMEOUT after table send"); sys.exit(1)
    print(f"  RX: {resp.hex(' ')} (should be ANS_OK=01, version=02)")
print()

# Step 8: Re-enter debug (in case the proxy didn't do it)
print("[8] ENTER (re-enter after table update)")
r = send_cmd(ser, "ENTER", CMD_ENTER)
print()

# Step 9: Chip ID again (with CC2510 table)
print("[9] CHIP_ID (should still work)")
r = send_cmd(ser, "CHIP_ID", CMD_CHIP_ID)
if r and r[0] == 1:
    chip_id = (r[1] << 8) | r[2]
    print(f"  Chip ID: 0x{chip_id:04x}")
print()

# Step 10: Status with CC2510 table
print("[10] STATUS (CC2510 table)")
r = send_cmd(ser, "STATUS", CMD_STATUS)
if r and r[0] == 1:
    status = r[2]
    print(f"  Status: 0x{status:02x}")
    if status & 0x04: print("  DEBUG_LOCKED!")
    if status & 0x20: print("  CPU_HALTED")
    if status & 0x02: print("  OSC_STABLE")
print()

# Step 11: Test EXEC_3 (MOV DPTR, #0x0000)
print("[11] EXEC_3: MOV DPTR, #0x0000")
r = send_cmd(ser, "EXEC_3", CMD_EXEC_3, 0x90, 0x00, 0x00)
print()

# Step 12: Test EXEC_1 (CLR A) - this is where it was failing!
print("[12] EXEC_1: CLR A (0xE4) - THIS IS THE FAILING COMMAND")
r = send_cmd(ser, "EXEC_1", CMD_EXEC_1, 0xE4)
print()

# Step 13: Test EXEC_1 (MOVC A, @A+DPTR)
print("[13] EXEC_1: MOVC A,@A+DPTR (0x93)")
r = send_cmd(ser, "EXEC_1", CMD_EXEC_1, 0x93)
if r and r[0] == 1:
    print(f"  Flash byte at 0x0000: 0x{r[2]:02x}")
print()

# Step 14: Read a few more bytes
print("[14] Reading first 16 bytes of flash...")
for i in range(15):
    r = send_cmd(ser, f"EXEC_1 (INC DPTR)", CMD_EXEC_1, 0xA3)
    r = send_cmd(ser, f"EXEC_1 (CLR A)", CMD_EXEC_1, 0xE4)
    r = send_cmd(ser, f"EXEC_1 (MOVC)", CMD_EXEC_1, 0x93)
    if r is None:
        print("  TIMEOUT - stopping"); break
    if r[0] == 1:
        print(f"  Byte {i+1}: 0x{r[2]:02x}")

print("\nDone!")
ser.close()
