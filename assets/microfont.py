#!/usr/bin/env python3
import sys
from PIL import Image
import argparse

def load_ascii_mapping(filename):
    """
    Reads the ascii.txt file and returns a list of characters,
    removing any newline characters.
    """
    with open(filename, "r", encoding="utf-8") as f:
        content = f.read()
    content = content.replace("\n", "").replace("\r", "")
    return list(content)

def load_phrases(filename):
    """
    Reads the phrases files and returns each line
    """
    with open(filename, "r", encoding="utf-8") as f:
        phrases = f.readlines()
    phrases = [phrase.strip("\n") for phrase in phrases]
    return phrases

def extract_required_chars(ascii_mapping, phrase):
    """
    Extracts the unique (non-whitespace) characters required to write the phrase.
    The order of the characters is the same as they appear in the mapping.
    """
    required = []
    for char in ascii_mapping:
        if char.isspace():
            continue
        if char in phrase and char not in required:
            required.append(char)
    return required

def extract_sprite(font_img, index, border_w, border_h, cell_width, cell_height, transparent):
    """
    Given the index of the sprite (in grid order, left-to-right, top-to-bottom),
    extracts the corresponding cell from the font image.
    """
    cols = font_img.width // cell_width
    col = index % cols
    row = index // cols
    x = col * cell_width
    y = row * cell_height
    sprite = font_img.crop((x + border_w, y + border_h, x + cell_width, y + cell_height))
    # Convert non-transparent pixels to white
    sprite = sprite.convert("RGBA")
    data = sprite.getdata()
    new_data = []
    for item in data:
        if item != transparent:
            new_data.append((255, 255, 255, 255))
        else:
            new_data.append(item)
    sprite.putdata(new_data)
    return sprite

def common_crop_bounds(sprites):
    """
    Determines the common transparent borders among all sprites.
    Returns a tuple (left_offset, top_offset, new_width, new_height).
    """
    if not sprites:
        return (0, 0, 0, 0)
    w, h = sprites[0].size

    # Determine top offset
    top_offset = 0
    for y in range(h):
        all_transparent = True
        for sprite in sprites:
            for x in range(w):
                if sprite.getpixel((x, y))[3] != 0:
                    all_transparent = False
                    break
            if not all_transparent:
                break
        if all_transparent:
            top_offset += 1
        else:
            break

    # Determine bottom offset
    bottom_offset = 0
    for y in range(h - 1, -1, -1):
        all_transparent = True
        for sprite in sprites:
            for x in range(w):
                if sprite.getpixel((x, y))[3] != 0:
                    all_transparent = False
                    break
            if not all_transparent:
                break
        if all_transparent:
            bottom_offset += 1
        else:
            break

    # Determine left offset
    left_offset = 0
    for x in range(w):
        all_transparent = True
        for sprite in sprites:
            for y in range(h):
                if sprite.getpixel((x, y))[3] != 0:
                    all_transparent = False
                    break
            if not all_transparent:
                break
        if all_transparent:
            left_offset += 1
        else:
            break

    # Determine right offset
    right_offset = 0
    for x in range(w - 1, -1, -1):
        all_transparent = True
        for sprite in sprites:
            for y in range(h):
                if sprite.getpixel((x, y))[3] != 0:
                    all_transparent = False
                    break
            if not all_transparent:
                break
        if all_transparent:
            right_offset += 1
        else:
            break

    new_width = w - left_offset - right_offset
    new_height = h - top_offset - bottom_offset
    return (left_offset, top_offset, new_width, new_height)

def create_atlas(font_img, ascii_mapping, required_chars, border_w, border_h, cell_width, cell_height):
    """
    Creates an atlas image in a single vertical column containing only the characters
    required to write the phrase. For each required character, it extracts the sprite
    from the font image (using the first occurrence in the mapping). Then, if all sprites
    have common transparent borders, they are cropped uniformly.
    
    Returns the atlas image, the new cell dimensions (new_width, new_height),
    and the list of cropped sprites.
    """
    # Fetch trasnsaprent pixel: top-left corner after border
    font_img = font_img.convert("RGBA")
    transparent = font_img.getpixel((border_w, border_h))

    sprites = []
    for char in required_chars:
        try:
            index = ascii_mapping.index(char)
        except ValueError:
            print(f"Warning: character '{char}' not found in the mapping!")
            continue
        sprite = extract_sprite(font_img, index, border_w, border_h, cell_width, cell_height, transparent)
        sprites.append(sprite)
    
    # Determine common crop bounds across all sprites
    left_offset, top_offset, new_width, new_height = common_crop_bounds(sprites)
    
    # Crop all sprites using the common bounds
    cropped_sprites = []
    for sprite in sprites:
        cropped = sprite.crop((left_offset, top_offset, left_offset + new_width, top_offset + new_height))
        cropped_sprites.append(cropped)
    
    # Print new dimensions to stdout
    print(f"New character dimensions after reduction: {new_width}x{new_height}")
    
    # Create atlas as a vertical column
    atlas_width = new_width
    atlas_height = len(cropped_sprites) * new_height
    atlas = Image.new("RGBA", (atlas_width, atlas_height), (0, 0, 0, 0))
    for i, sprite in enumerate(cropped_sprites):
        atlas.paste(sprite, (0, i * new_height))
    return atlas, (new_width, new_height), cropped_sprites

def compute_sprite_width(sprite):
    """
    Computes the width of a sprite by finding the rightmost non-transparent pixel.
    Returns the width as the index (plus one) of that pixel.
    """
    w, h = sprite.size
    width_found = 0
    for x in range(w - 1, -1, -1):
        found = False
        for y in range(h):
            if sprite.getpixel((x, y))[3] != 0:
                found = True
                break
        if found:
            width_found = x + 1  # +1 to get the pixel count
            break
    return width_found

def create_phrase_binary(phrase, required_chars):
    """
    Converts the phrase into a binary file:
      - For each non-whitespace character, writes a byte corresponding to the 1-based
        index of the character in the required_chars list.
      - For each whitespace character, writes 0.
    """
    data = bytearray()
    for c in phrase:
        if c.isspace():
            data.append(0)
        else:
            try:
                idx = required_chars.index(c) + 1  # 1-based index
            except ValueError:
                idx = 0
            data.append(idx)
    return data

def main():
    if len(sys.argv) < 4:
        print("Usage: python script.py font.png ascii.txt phrase.txt")
        sys.exit(1)

    # Parse command-line arguments using argparse.
    parser = argparse.ArgumentParser(description="Generate a sprite atlas and binary files from a font image and text phrases.")
    parser.add_argument("font", help="Path to the font image (PNG format).")
    parser.add_argument("ascii", help="Path to the ASCII mapping file.")
    parser.add_argument("border_w", type=int, help="Width of the border around each character cell.")
    parser.add_argument("border_h", type=int, help="Height of the border around each character cell.")
    parser.add_argument("cell_width", type=int, help="Width of each character cell.")
    parser.add_argument("cell_height", type=int, help="Height of each character cell.")
    parser.add_argument("--phrases", type=str, required=True, help="Text file containing phrases to convert (one per line).")

    args = parser.parse_args()

    font_filename = args.font
    ascii_filename = args.ascii
    border_w = args.border_w
    border_h = args.border_h
    cell_width = args.cell_width
    cell_height = args.cell_height
    phrases_filename = args.phrases

    # Load the font image
    font_img = Image.open(font_filename).convert("RGBA")
    
    # Load the ascii mapping and the phrase to be represented
    ascii_mapping = load_ascii_mapping(ascii_filename)

    # Load all the phrases
    phrases = load_phrases(phrases_filename)
    
    # Extract the unique characters required for the phrase (excluding whitespace)
    required_chars = extract_required_chars(ascii_mapping, "".join(phrases))
    print("Required characters:", required_chars)
    
    # Create the atlas with the required characters, arranged in a single vertical column,
    # applying common cropping if possible.
    atlas, new_dims, cropped_sprites = create_atlas(font_img, ascii_mapping, required_chars, border_w, border_h, cell_width, cell_height)
    atlas.save("output_atlas.png")
    print("Atlas saved as output_atlas.png")

    # Create an output file which is a C source file. Encode the atlas as an array of
    # bytes where each byte encodes 2 pixels (4 bits each). Each pixel is either 0 or 0xF.
    with open("font.c.inc", "w") as f:
        assert(new_dims[0] % 2 == 0)  # Ensure the width is even
        char_size = ((new_dims[0]+1)//2 * new_dims[1] + 7) // 8 * 8
        padding_size = char_size - ((new_dims[0]+1)//2 * new_dims[1])
        f.write("#define CHAR_WIDTH %d\n" % new_dims[0])
        f.write("#define CHAR_HEIGHT %d\n" % new_dims[1])
        f.write("#define CHAR_SIZE %d\n" % char_size)
        f.write("__attribute__((aligned(8)))\n")
        f.write(f"const unsigned char font[] = {{\n")
        data = atlas.convert("L").tobytes()
        for i in range(0, len(data), 2):
            if i % (new_dims[0]) == 0:
                f.write("    ")
            # Get two pixels and encode them into a single byte
            pixel1 = 0xF if data[i] > 0 else 0x0
            pixel2 = 0xF if (i + 1 < len(data) and data[i + 1] > 0) else 0x0
            byte = (pixel1 << 4) | pixel2
            f.write(f"0x{byte:02X}, ")
            if (i+2) % new_dims[0] == 0:
                f.write("\n")
                if (i+2) % (new_dims[0] * new_dims[1]) == 0:
                    if padding_size > 0:
                        f.write("    ")
                        for _ in range(padding_size):
                            f.write("0x00, ")
                        f.write("  // padding\n")
                    f.write("\n")
        f.write("};\n")

        # Additional step:
        # For each cropped sprite, compute its width as the index of the rightmost non-transparent pixel (+1)
        # Then, create an output array that contains:
        #   - The first byte: the grid width (new_width) to be used as the spacing dimension.
        #   - The following bytes: the widths of all characters in the atlas, one per byte.
        widths = []
        widths.append(new_dims[0])  # Use new_width as the spacing dimension
        min_width = 0xFF
        for sprite in cropped_sprites:
            w = compute_sprite_width(sprite)
            if w < min_width: min_width = w
            widths.append(w+1)

        min_width = min_width + 1
        
        f.write(f"#define CHAR_SPACING_OFFSET {min_width}\n")

        # 3 bits for width
        assert min_width < 0b111

        # Create a single array with binary data for all the phrases, concatenating them.
        f.write("const unsigned char phrases[] = {\n")
        for phrase in phrases:
            phrase_data = create_phrase_binary(phrase, required_chars)
            f.write(f"    // {phrase}\n")
            for i, byte in enumerate(phrase_data):
                
                # 5 bits for char code
                assert byte < 0b11111

                packed_char = ((widths[byte] - min_width) << 5) | byte
                
                if i % 16 == 0:
                    f.write("    ")
                f.write(f"0x{packed_char:02X}, ")
                if (i + 1) % 16 == 0:
                    f.write("\n")
            f.write("\n")
        f.write("};\n")
        
        # Create an array with offsets for each phrase
        offset = 0
        f.write(f"const unsigned short phrases_off[] = {{\n")
        for phrase in phrases:
            f.write(f"    {offset}, // {phrase}\n")
            offset += len(phrase)
        f.write(f"    {offset}\n");
        f.write("};\n")

    print("C source file saved as font.c.inc")
    
if __name__ == "__main__":
    main()
