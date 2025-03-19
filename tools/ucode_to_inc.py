#!/usr/bin/env python3
import sys

def process_files(file1, file2, output_header):
    with open(file1, 'rb') as f1, open(file2, 'rb') as f2:
        data1 = f1.read()
        data2 = f2.read()

    padded1 = data1.ljust(4096, b'\x00')
    padded2 = data2.ljust(1024, b'\x00')
    merged_data = padded1 + padded2

    with open(output_header, 'w') as out:
        
        out.write(f"unsigned char rsp_data_code[] __attribute__((aligned(8))) = {{\n")
        
        for i, byte in enumerate(merged_data):
            out.write(f" 0x{byte:02X},")
            if (i + 1) % 16 == 0:
                out.write("\n")
        
        out.write("\n};\n")

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: python ucode_to_inc.py <file1> <file2> <output_header>")
        sys.exit(1)

    process_files(sys.argv[1], sys.argv[2], sys.argv[3])