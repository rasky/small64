#!/usr/bin/env python3
import os
import sys
import argparse

def main():
    parser = argparse.ArgumentParser(
        description="Generate a C source file containing an unsigned char array from a binary file."
    )
    parser.add_argument("input_file", help="Input binary file")
    parser.add_argument("output_file", nargs="?", help="Output C file (if omitted, output to stdout)")
    parser.add_argument("-n", "--name", help="Custom name for the generated variables")
    parser.add_argument("-a", "--aligned", type=int,
                        help="Alignment factor for the GCC attribute (e.g. 16 for aligned(16))")
    
    args = parser.parse_args()

    input_file = args.input_file
    output_file = args.output_file

    # Use the custom name if provided; otherwise, derive the variable name from the input file name.
    if args.name:
        var_name = args.name
    else:
        base_name = os.path.basename(input_file)
        var_name = base_name.replace(".", "_")

    # Read the binary file contents.
    try:
        with open(input_file, "rb") as f:
            data = f.read()
    except Exception as e:
        sys.exit(f"Error reading file {input_file}: {e}")

    # Build the C source.
    output_lines = []
    # Add the aligned attribute if an alignment factor is specified.
    if args.aligned is not None:
        output_lines.append("unsigned char {}[] __attribute__((aligned({}))) = {{".format(var_name, args.aligned))
    else:
        output_lines.append("unsigned char {}[] = {{".format(var_name))
    
    # Group 12 bytes per line.
    for i in range(0, len(data), 12):
        chunk = data[i:i+12]
        line = "  " + ", ".join("0x{:02x}".format(b) for b in chunk)
        if i + 12 < len(data):
            line += ","
        output_lines.append(line)
    output_lines.append("};")
    output_lines.append("const unsigned int {}_len = {};".format(var_name, len(data)))
    output_str = "\n".join(output_lines)

    # Write to output file if specified; otherwise, print to stdout.
    if output_file:
        try:
            with open(output_file, "w") as f:
                f.write(output_str)
        except Exception as e:
            sys.exit(f"Error writing file {output_file}: {e}")
    else:
        print(output_str)

if __name__ == "__main__":
    main()

