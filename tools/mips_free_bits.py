#!/usr/bin/env python3
"""
MIPS IV Free-Bit Scanner

This tool scans a flat binary file of 32-bit big-endian MIPS IV instructions,
identifies reserved ("free") bits in each opcode, and reports their locations.

Usage:
    python3 mips_free_bits_tool.py [-v|--verbose] [--base OFFSET] [--limit N] <filename>

Options:
    -v, --verbose   Print a summary table of opcode counts and free-bit totals.
    --base OFFSET   Byte-index base for reporting (decimal or hex, default=0).
    --limit N       Maximum number of free bits to report in the list (default: no limit).
"""

import argparse
import struct
import sys

# Mapping of (primary_opcode, funct_or_rs) to list of free bit positions (0 = LSB)
OPCODE_FREE_BITS = {
    (0x00, 0x04): list(range(6, 11)),  # sllv
    (0x00, 0x06): list(range(6, 11)),  # srlv
    (0x00, 0x07): list(range(6, 11)),  # srav
    (0x00, 0x00): list(range(21, 26)),  # sll
    (0x00, 0x02): list(range(21, 26)),  # srl
    (0x00, 0x03): list(range(21, 26)),  # sra
    (0x00, 0x08): list(range(6, 21)),   # jr
    (0x00, 0x09): list(range(6, 11)) + list(range(16, 21)),  # jalr
    (0x00, 0x20): list(range(6, 11)),   # add
    (0x00, 0x21): list(range(6, 11)),   # addu
    (0x00, 0x22): list(range(6, 11)),   # sub
    (0x00, 0x23): list(range(6, 11)),   # subu
    (0x00, 0x24): list(range(6, 11)),   # and
    (0x00, 0x25): list(range(6, 11)),   # or
    (0x00, 0x26): list(range(6, 11)),   # xor
    (0x00, 0x27): list(range(6, 11)),   # nor
    (0x00, 0x2A): list(range(6, 11)),   # slt
    (0x00, 0x2B): list(range(6, 11)),   # sltu
    (0x00, 0x14): list(range(6, 11)),   # dsllv
    (0x00, 0x16): list(range(6, 11)),   # dsrlv
    (0x00, 0x17): list(range(6, 11)),   # dsrav
    (0x00, 0x38): list(range(21, 26)),  # dsll
    (0x00, 0x3A): list(range(21, 26)),  # dsrl
    (0x00, 0x3B): list(range(21, 26)),  # dsra
    (0x00, 0x2C): list(range(6, 11)),   # dadd
    (0x00, 0x2D): list(range(6, 11)),   # daddu
    (0x00, 0x2E): list(range(6, 11)),   # dsub
    (0x00, 0x2F): list(range(6, 11)),   # dsubu
    (0x0F, None):   list(range(21, 26)), # lui
    (0x10, 0x00): list(range(0, 11)),   # mfc0
    (0x10, 0x02): list(range(0, 11)),   # cfc0
    (0x10, 0x04): list(range(0, 11)),   # mtc0
    (0x10, 0x06): list(range(0, 11)),   # ctc0
    (0x11, 0x00): list(range(0, 11)),   # mfc1
    (0x11, 0x01): list(range(0, 11)),   # dmfc1
    (0x11, 0x02): list(range(0, 11)),   # cfc1
    (0x11, 0x04): list(range(0, 11)),   # mtc1
    (0x11, 0x05): list(range(0, 11)),   # dmtc1
    (0x11, 0x06): list(range(0, 11)),   # ctc1
}

# Human-readable mnemonics for verbose mode
OPCODE_MNEMONICS = {
    (0x00, 0x04): 'sllv',  (0x00, 0x06): 'srlv',  (0x00, 0x07): 'srav',
    (0x00, 0x00): 'sll',   (0x00, 0x02): 'srl',   (0x00, 0x03): 'sra',
    (0x00, 0x08): 'jr',    (0x00, 0x09): 'jalr',
    (0x00, 0x20): 'add',   (0x00, 0x21): 'addu',  (0x00, 0x22): 'sub',
    (0x00, 0x23): 'subu',  (0x00, 0x24): 'and',   (0x00, 0x25): 'or',
    (0x00, 0x26): 'xor',   (0x00, 0x27): 'nor',   (0x00, 0x2A): 'slt',
    (0x00, 0x2B): 'sltu',  (0x00, 0x14): 'dsllv', (0x00, 0x16): 'dsrlv',
    (0x00, 0x17): 'dsrav', (0x00, 0x38): 'dsll',  (0x00, 0x3A): 'dsrl',
    (0x00, 0x3B): 'dsra',  (0x00, 0x2C): 'dadd',  (0x00, 0x2D): 'daddu',
    (0x00, 0x2E): 'dsub',  (0x00, 0x2F): 'dsubu', (0x0F, None): 'lui',
    (0x10, 0x00): 'mfc0',  (0x10, 0x02): 'cfc0',  (0x10, 0x04): 'mtc0',
    (0x10, 0x06): 'ctc0',  (0x11, 0x00): 'mfc1',  (0x11, 0x01): 'dmfc1',
    (0x11, 0x02): 'cfc1',  (0x11, 0x04): 'mtc1',  (0x11, 0x05): 'dmtc1',
    (0x11, 0x06): 'ctc1',
}


def parse_args():
    parser = argparse.ArgumentParser(description="Scan MIPS IV binary for free bits.")
    parser.add_argument('filename', help='Path to the binary file')
    parser.add_argument('-v', '--verbose', action='store_true', help='Verbose summary')
    parser.add_argument('--base', default='0', help='Base offset (decimal or hex)')
    parser.add_argument('--limit', type=int, help='Maximum number of free bits to report')
    return parser.parse_args()


def main():
    args = parse_args()
    try:
        base = int(args.base, 0)
    except ValueError:
        print(f"Invalid base offset: {args.base}", file=sys.stderr)
        sys.exit(1)

    try:
        with open(args.filename, 'rb') as f:
            data = f.read()
    except IOError as e:
        print(f"Error opening file: {e}", file=sys.stderr)
        sys.exit(1)

    length = len(data)
    if length % 4 != 0:
        print("Warning: file size is not a multiple of 4 bytes", file=sys.stderr)

    opcode_stats = {}
    free_bits_global = {}

    # Scan each 32-bit instruction
    for idx in range(length // 4):
        offset = idx * 4
        instr, = struct.unpack('>I', data[offset:offset+4])
        primary = (instr >> 26) & 0x3F
        if primary == 0x00:
            funct = instr & 0x3F
            key = (primary, funct)
        elif primary in (0x10, 0x11):
            rs = (instr >> 21) & 0x1F
            key = (primary, rs)
        else:
            key = (primary, None)

        # Determine mnemonic
        mnemonic = OPCODE_MNEMONICS.get(key) or (
            f"R_{key[1]:02X}" if primary == 0x00 else
            f"COP{primary-0x0F}_{key[1]:02X}" if primary in (0x10, 0x11) else
            f"OP_{primary:02X}"
        )

        free_bits = OPCODE_FREE_BITS.get(key, [])
        # Verbose accumulation
        if args.verbose and free_bits:
            stats = opcode_stats.setdefault(mnemonic, {'count': 0, 'free_bits': 0})
            stats['count'] += 1
            stats['free_bits'] += len(free_bits)

        # Accumulate global free bit positions
        for bit in free_bits:
            global_bit = idx * 32 + bit
            byte_idx = base + (global_bit // 8)
            bit_pos = global_bit % 8
            free_bits_global.setdefault(byte_idx, set()).add(bit_pos)

    # Print verbose summary
    if args.verbose:
        total_free = sum(stats['free_bits'] for stats in opcode_stats.values())
        print(f"{'Mnemonic':<10} {'Count':>8} {'FreeBits':>10}")
        print('-' * 30)
        for mnem, stats in sorted(opcode_stats.items()):
            print(f"{mnem:<10} {stats['count']:>8} {stats['free_bits']:>10}")
        print(f"Total free bits: {total_free}\n")

    # Flatten and apply limit
    flat_bits = []
    for byte_idx in sorted(free_bits_global):
        for bit in sorted(free_bits_global[byte_idx]):
            flat_bits.append((byte_idx, bit))
    if args.limit is not None:
        flat_bits = flat_bits[:args.limit]

    # Group by byte for output ranges
    grouped = {}
    for byte_idx, bit in flat_bits:
        grouped.setdefault(byte_idx, []).append(bit)

    # Build output entries
    entries = []
    for byte_idx in sorted(grouped):
        bits = sorted(grouped[byte_idx])
        ranges = []
        start = prev = bits[0]
        for b in bits[1:]:
            if b == prev + 1:
                prev = b
            else:
                ranges.append((start, prev))
                start = prev = b
        ranges.append((start, prev))

        for a, b in ranges:
            if a == 0 and b == 7:
                entries.append(f"{byte_idx}")
            elif a == b:
                entries.append(f"{byte_idx}[{a}]")
            else:
                entries.append(f"{byte_idx}[{a}..{b}]")

    # Print final bit list
    print(','.join(entries))


if __name__ == '__main__':
    main()
