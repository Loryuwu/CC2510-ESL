"""Convert Intel HEX (.ihx) to binary for CC2510 flash writer."""
import sys

def hex2bin(hexfile, binfile):
    data = {}
    with open(hexfile, 'r') as f:
        for line in f:
            line = line.strip().replace(' ', '')
            if not line.startswith(':'):
                continue
            hexstr = line[1:]
            if len(hexstr) < 10:
                continue
            rec = bytes.fromhex(hexstr)
            length = rec[0]
            addr = (rec[1] << 8) | rec[2]
            rtype = rec[3]
            if rtype == 0:  # Data record
                for i in range(length):
                    data[addr + i] = rec[4 + i]
            elif rtype == 1:  # EOF
                break

    if not data:
        print("No data found!"); sys.exit(1)

    min_addr = min(data.keys())
    max_addr = max(data.keys())
    size = max_addr - min_addr + 1

    binary = bytearray(size)
    for i in range(size):
        binary[i] = data.get(min_addr + i, 0xFF)

    with open(binfile, 'wb') as f:
        f.write(binary)

    print(f"Converted: {len(data)} bytes, range 0x{min_addr:04X}-0x{max_addr:04X}, output {size} bytes")

if __name__ == '__main__':
    hex2bin(sys.argv[1], sys.argv[2])
