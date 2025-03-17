#!/usr/bin/env python3
import sys
from PIL import Image

def load_ascii_mapping(filename):
    """
    Reads the ascii.txt file and returns a list of characters,
    removing any newline characters.
    """
    with open(filename, "r", encoding="utf-8") as f:
        content = f.read()
    content = content.replace("\n", "").replace("\r", "")
    return list(content)

def load_phrase(filename):
    """
    Reads the phrase.txt file and returns the contained string.
    """
    with open(filename, "r", encoding="utf-8") as f:
        phrase = f.read()
    return phrase

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
    
    font_filename = sys.argv[1]
    ascii_filename = sys.argv[2]
    border_w = int(sys.argv[3])
    border_h = int(sys.argv[4])
    cell_width = int(sys.argv[5])
    cell_height = int(sys.argv[6])
    phrase_filename = sys.argv[7]

    # Load the font image
    font_img = Image.open(font_filename).convert("RGBA")
    
    # Load the ascii mapping and the phrase to be represented
    ascii_mapping = load_ascii_mapping(ascii_filename)
    phrase = load_phrase(phrase_filename)
    
    # Extract the unique characters required for the phrase (excluding whitespace)
    required_chars = extract_required_chars(ascii_mapping, phrase)
    print("Required characters:", required_chars)
    
    # Create the atlas with the required characters, arranged in a single vertical column,
    # applying common cropping if possible.
    atlas, new_dims, cropped_sprites = create_atlas(font_img, ascii_mapping, required_chars, border_w, border_h, cell_width, cell_height)
    atlas.save("output_atlas.png")
    print("Atlas saved as output_atlas.png")
    
    # Create the binary file representing the phrase:
    # For each character (0 for whitespace, otherwise the 1-based index in the atlas)
    phrase_data = create_phrase_binary(phrase, required_chars)
    with open("phrase.bin", "wb") as f:
        f.write(phrase_data)
    print("Binary phrase file saved as phrase.bin")
    
    # Additional step:
    # For each cropped sprite, compute its width as the index of the rightmost non-transparent pixel (+1)
    # Then, create an output binary file that contains:
    #   - The first byte: the grid width (new_width) to be used as the spacing dimension.
    #   - The following bytes: the widths of all characters in the atlas, one per byte.
    widths = []
    grid_spacing = new_dims[0]  # Use new_width as the spacing dimension
    widths.append(grid_spacing - 3)
    for sprite in cropped_sprites:
        w = compute_sprite_width(sprite)
        widths.append(w+1)
    with open("widths.bin", "wb") as f:
        f.write(bytearray(widths))
    print("Widths file saved as widths.bin")
    
if __name__ == "__main__":
    main()
