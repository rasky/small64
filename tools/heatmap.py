#!/usr/bin/env python3
import sys
import argparse
import json
from elftools.elf.elffile import ELFFile

def main():
    parser = argparse.ArgumentParser(
        description="Dump ELF symbols from specified sections with optional heatmap for compression data."
    )
    parser.add_argument("elf_file", help="ELF file to process")
    parser.add_argument("sections", nargs="+", help="Section names to process")
    parser.add_argument("--heatmap", help="Optional binary heatmap file for compression data")
    parser.add_argument("--json", action="store_true", help="Dump output in JSON format")
    parser.add_argument("--html", action="store_true", help="Generate interactive HTML output with heatmap visualization")
    args = parser.parse_args()

    filename = args.elf_file
    section_names = args.sections
    heatmap_filename = args.heatmap

    with open(filename, "rb") as f:
        elf = ELFFile(f)

        # Build an ordered list of target sections (tuple: (section index, section object))
        target_sections_ordered = []
        for sec_name in section_names:
            section = elf.get_section_by_name(sec_name)
            if section is None:
                print(f"Warning: Section {sec_name} not found in {filename}.")
                continue
            sec_index = None
            for idx, sec in enumerate(elf.iter_sections()):
                if sec == section:
                    sec_index = idx
                    break
            if sec_index is None:
                print(f"Could not determine index for section {sec_name}.")
                continue
            target_sections_ordered.append((sec_index, section))
        
        if not target_sections_ordered:
            print("No valid target sections found.")
            sys.exit(1)

        # Compute cumulative offsets for each section as if they were concatenated.
        cumulative_offsets = {}
        cumulative = 0
        for sec_index, section in target_sections_ordered:
            cumulative_offsets[sec_index] = cumulative
            cumulative += section.header['sh_size']
        total_sections_size = cumulative

        # Load heatmap if provided and check size.
        heatmap_data = None
        if heatmap_filename:
            try:
                with open(heatmap_filename, "rb") as hf:
                    heatmap_data = hf.read()
                if len(heatmap_data) != total_sections_size:
                    print(f"Error: Heatmap file size ({len(heatmap_data)}) does not match total sections size ({total_sections_size}).")
                    sys.exit(1)
            except Exception as e:
                print(f"Error reading heatmap file: {e}")
                sys.exit(1)

        # Build a mapping from section index to section object.
        target_sections_dict = {sec_index: section for sec_index, section in target_sections_ordered}

        # Locate the symbol table (usually .symtab)
        symtab = elf.get_section_by_name('.symtab')
        if symtab is None:
            print(f"No symbol table found in {filename}.")
            sys.exit(1)
        
        symbols_info = []
        for symbol in symtab.iter_symbols():
            # Skip symbols with non-integer st_shndx (e.g., SHN_UNDEF) and with size 0.
            if not isinstance(symbol['st_shndx'], int) or symbol['st_size'] == 0:
                continue
            sec_index = symbol['st_shndx']
            if sec_index not in cumulative_offsets:
                continue  # Skip symbols not in one of our target sections.
            
            section = target_sections_dict[sec_index]
            # Compute the relative offset within its section.
            rel_offset = symbol['st_value'] - section.header['sh_addr']
            # Global offset is the section's cumulative offset plus its relative offset.
            global_offset = cumulative_offsets[sec_index] + rel_offset

            sym_dict = {
                'section': section.name,
                'name': symbol.name,
                'type': symbol['st_info']['type'],
                'global_offset': global_offset,
                'size': symbol['st_size']
            }
            # If a heatmap is provided, compute compressed size and ratio.
            if heatmap_data is not None:
                comp_bits = 0.0
                start = global_offset
                end = global_offset + symbol['st_size']
                for i in range(start, end):
                    b = heatmap_data[i]
                    comp_bits += 2 ** (((b >> 1) - 64) / 8.0)
                comp_size = comp_bits / 8.0  # Convert bits to bytes.
                ratio = comp_size / symbol['st_size']
                sym_dict['comp_size'] = comp_size
                sym_dict['ratio'] = ratio

            symbols_info.append(sym_dict)

        # Branch by output mode.
        if args.html:
            # HTML visualization: sort by global offset.
            sorted_symbols = sorted(symbols_info, key=lambda s: s['global_offset'])
            # Compute cumulative compressed size for each symbol.
            total_comp = sum(s['comp_size'] for s in sorted_symbols)
            cum = 0.0
            for s in sorted_symbols:
                s['cum_comp'] = cum
                cum += s['comp_size']
            # Assign colors from a preset palette.
            palette = ["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728",
                       "#9467bd", "#8c564b", "#e377c2", "#7f7f7f",
                       "#bcbd22", "#17becf"]
            for i, s in enumerate(sorted_symbols):
                s['color'] = palette[i % len(palette)]
            # Use percentage positions so that the bar spans the full browser width.
            BAR_HEIGHT = 50   # pixels
            for s in sorted_symbols:
                s['left_pct'] = (s['cum_comp'] / total_comp) * 100
                s['width_pct'] = (s['comp_size'] / total_comp) * 100

            # Generate HTML output with clickable segments.
            html = f"""<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>ELF Heatmap Visualization</title>
<style>
  body {{
    font-family: Arial, sans-serif;
    margin: 20px;
  }}
  #heatmap-bar {{
    position: relative;
    width: 100%;
    height: {BAR_HEIGHT}px;
    border: 1px solid #000;
    margin: 20px 0;
  }}
  .symbol {{
    position: absolute;
    height: 100%;
    overflow: hidden;
    white-space: nowrap;
    text-overflow: ellipsis;
    text-align: center;
    line-height: {BAR_HEIGHT}px;
    font-size: 12px;
    color: white;
    box-sizing: border-box;
    cursor: pointer;
  }}
  #symbol-info {{
    margin-top: 20px;
    padding: 10px;
    border: 1px solid #ccc;
  }}
</style>
</head>
<body>
<h1>ELF Heatmap Visualization</h1>
<div id="heatmap-bar">
"""
            for s in sorted_symbols:
                data_attrs = (
                    f'data-name="{s["name"]}" '
                    f'data-type="{s["type"]}" '
                    f'data-size="{s["size"]}" '
                    f'data-comp="{s["comp_size"]:.2f}" '
                    f'data-ratio="{s["ratio"]:.2f}" '
                    f'data-offset="0x{s["global_offset"]:x}"'
                )
                html += f'<div class="symbol" {data_attrs} onclick="showInfo(this)" style="left: {s["left_pct"]:.2f}%; width: {s["width_pct"]:.2f}%; background-color: {s["color"]};">{s["name"]}</div>\n'
            html += f"""</div>
<div id="symbol-info"><em>Click on a segment to see details.</em></div>
<script>
function showInfo(elem) {{
  var infoDiv = document.getElementById("symbol-info");
  var name = elem.getAttribute("data-name");
  var type = elem.getAttribute("data-type");
  var size = elem.getAttribute("data-size");
  var comp = elem.getAttribute("data-comp");
  var ratio = elem.getAttribute("data-ratio");
  var offset = elem.getAttribute("data-offset");
  infoDiv.innerHTML = "<p><strong>" + name + "</strong> (Type: " + type + ")<br/>Offset: " + offset +
                      ", Size: " + size + " bytes, CompSize: " + comp + " bytes, Ratio: " + ratio + "</p>";
}}
</script>
</body>
</html>
"""
            print(html)
            sys.exit(0)
        elif args.json:
            # For JSON (non-HTML) output, sort by compressed size if heatmap provided; otherwise by original size.
            if heatmap_data is not None:
                symbols_info = sorted(symbols_info, key=lambda s: s['comp_size'], reverse=True)
            else:
                symbols_info = sorted(symbols_info, key=lambda s: s['size'], reverse=True)
            output = []
            for sym in symbols_info:
                out_dict = {
                    "Section": sym["section"],
                    "Name": sym["name"],
                    "Type": sym["type"],
                    "Offset": "0x{:x}".format(sym["global_offset"]),
                    "Size": sym["size"]
                }
                if heatmap_data is not None:
                    out_dict["CompSize"] = round(sym["comp_size"], 2)
                    out_dict["Ratio"] = round(sym["ratio"], 2)
                output.append(out_dict)
            print(json.dumps(output, indent=2))
            sys.exit(0)
        else:
            # For text table output, sort by compressed size (if available) or original size.
            if heatmap_data is not None:
                symbols_info = sorted(symbols_info, key=lambda s: s['comp_size'], reverse=True)
            else:
                symbols_info = sorted(symbols_info, key=lambda s: s['size'], reverse=True)
        
        # Print text table.
        if heatmap_data is not None:
            header = "{:<16} {:<30} {:<12} {:<15} {:<10} {:<15} {:<10}".format(
                "Section", "Name", "Type", "Offset", "Size", "CompSize", "Ratio")
        else:
            header = "{:<16} {:<30} {:<12} {:<15} {:<10}".format(
                "Section", "Name", "Type", "Offset", "Size")
        print(header)
        print("-" * len(header))
        for sym in symbols_info:
            offset_str = "0x{:x}".format(sym['global_offset'])
            if heatmap_data is not None:
                comp_size_str = f"{sym['comp_size']:.2f}"
                ratio_str = f"{sym['ratio']:.2f}"
                row = "{:<16} {:<30} {:<12} {:<15} {:<10d} {:<15} {:<10}".format(
                    sym['section'],
                    sym['name'],
                    sym['type'],
                    offset_str,
                    sym['size'],
                    comp_size_str,
                    ratio_str
                )
            else:
                row = "{:<16} {:<30} {:<12} {:<15} {:<10d}".format(
                    sym['section'],
                    sym['name'],
                    sym['type'],
                    offset_str,
                    sym['size']
                )
            print(row)

if __name__ == '__main__':
    main()
