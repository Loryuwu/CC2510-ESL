"""
Generate a minimal CC2510 LED blink test firmware.
Hand-assembled 8051 code.

Hardware:
  P2.1 = White LED
  P2.2 = TPS61071 boost enable (powers LED)
  P0.0 = EPD power (active low)

This firmware blinks the white LED to confirm the chip is alive.
"""

firmware = bytearray(0x60)  # 96 bytes, rest will be 0xFF padding
for i in range(len(firmware)):
    firmware[i] = 0xFF

# Reset vector at 0x0000: LJMP main (0x0030)
firmware[0x00] = 0x02  # LJMP
firmware[0x01] = 0x00
firmware[0x02] = 0x30

# Main code at 0x0030
code = bytearray([
    # Set P2.1 and P2.2 as output
    0x75, 0xFF, 0x06,   # MOV P2DIR, #0x06

    # -- blink loop (offset 0x0033) --
    # LED ON
    0x75, 0xA0, 0x06,   # MOV P2, #0x06 (P2.1=1 LED, P2.2=1 boost)

    # Delay ~200ms at 26MHz
    0x7F, 0x00,          # MOV R7, #0
    0x7E, 0x00,          # MOV R6, #0
    0x7D, 0x06,          # MOV R5, #6
    # delay1 loop:
    0xDF, 0xFE,          # DJNZ R7, $ (self)
    0xDE, 0xFC,          # DJNZ R6, $-4
    0xDD, 0xFA,          # DJNZ R5, $-6

    # LED OFF (boost stays on)
    0x75, 0xA0, 0x04,   # MOV P2, #0x04 (P2.1=0 LED off, P2.2=1 boost on)

    # Delay ~200ms
    0x7F, 0x00,          # MOV R7, #0
    0x7E, 0x00,          # MOV R6, #0
    0x7D, 0x06,          # MOV R5, #6
    # delay2 loop:
    0xDF, 0xFE,          # DJNZ R7, $
    0xDE, 0xFC,          # DJNZ R6, $-4
    0xDD, 0xFA,          # DJNZ R5, $-6

    # Jump back to blink
    0x80, 0xE0,          # SJMP blink (-32 = 0xE0)
])

# Place code at 0x0030
for i, b in enumerate(code):
    firmware[0x30 + i] = b

# Verify SJMP target
sjmp_addr = 0x30 + len(code) - 2  # Address of SJMP instruction
sjmp_next = sjmp_addr + 2          # Next instruction after SJMP
blink_addr = 0x33                  # Target (blink label)
offset = blink_addr - sjmp_next
print(f"SJMP at 0x{sjmp_addr:04X}, target 0x{blink_addr:04X}, offset {offset} (0x{offset & 0xFF:02X})")
assert firmware[sjmp_addr + 1] == (offset & 0xFF), "SJMP offset wrong!"

# Save
outfile = "test_blink.bin"
with open(outfile, "wb") as f:
    f.write(firmware)

print(f"Firmware written to {outfile} ({len(firmware)} bytes)")
print(f"Flash with: python cc_write_direct.py COM28 {outfile}")

# Show hex dump
print("\nHex dump:")
for addr in range(0, len(firmware), 16):
    hex_str = " ".join(f"{firmware[addr+i]:02X}" for i in range(min(16, len(firmware)-addr)))
    print(f"  {addr:04X}: {hex_str}")
