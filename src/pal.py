# Define the palettes
teal_palette =      [0x0188, 0x0198, 0x0199, 0x01b9, 0x01bb, 0x02bb, 0x03bb, 0x03cb, 0x03cc]
plum_palette =      [0x0539, 0x0549, 0x054a, 0x056a, 0x056c, 0x066c, 0x076c, 0x077c, 0x077d]
maroon_palette =    [0x0633, 0x0643, 0x0644, 0x0664, 0x0666, 0x0766, 0x0866, 0x0876, 0x0877]
mossgreen_palette = [0x0060, 0x0171, 0x0281, 0x0492, 0x05A3, 0x07C4, 0x08D6, 0x0AE7, 0x0CF9]



# Function to extract RGB components from a 12-bit color
def extract_rgb(color):
    r = (color >> 8) & 0xF
    g = (color >> 4) & 0xF
    b = color & 0xF
    return r, g, b

# Function to combine RGB components into a 12-bit color
def combine_rgb(r, g, b):
    return ((r & 0xF) << 8) | ((g & 0xF) << 4) | (b & 0xF)

# Function to generate the palette based on initial color and differences
def generate_palette(initial_color):
    differences = [
        (0, 1, 0), (0, 0, 1), (0, 2, 0),
        (0, 0, 2), (1, 0, 0), (1, 0, 0), (0, 1, 0), (0, 0, 1)
    ]

    palette = [initial_color]
    r, g, b = extract_rgb(initial_color)

    for dr, dg, db in differences:
        r = (r + dr) % 16
        g = (g + dg) % 16
        b = (b + db) % 16
        palette.append(combine_rgb(r, g, b))

    return palette

# Generate the palettes based on the initial colors
generated_teal_palette = generate_palette(0x0188)
generated_plum_palette = generate_palette(0x0539)
generated_maroon_palette = generate_palette(0x0633)
generated_mossgreen_palette = generate_palette(0x0060)

# Print the generated palettes
print("Generated Teal Palette:", [hex(color) for color in generated_teal_palette])
print("Generated Plum Palette:", [hex(color) for color in generated_plum_palette])
print("Generated Maroon Palette:", [hex(color) for color in generated_maroon_palette])
print("Generated Mossgreen Palette:", [hex(color) for color in generated_mossgreen_palette])

# Assertions to ensure the generated palettes match the expected palettes
assert generated_teal_palette == teal_palette, "Teal palette does not match!"
assert generated_plum_palette == plum_palette, "Plum palette does not match!"
assert generated_maroon_palette == maroon_palette, "Maroon palette does not match!"
assert generated_mossgreen_palette == mossgreen_palette, "Mossgreen palette does not match!"

print("All palettes match!")
