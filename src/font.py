import os
from PIL import Image

letter_w = 5
spacing_w = 3
letter_h = 9
pink = (255, 0, 255, 255)

prefix = "../data/font"

chars = '!\"#$%&\'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~'
chars_pinbottom = 'fgjpqy!.:'
chars_count = len(chars)

total_w = (chars_count * letter_w) + (spacing_w * (chars_count - 1))

print(f'Chars: {chars_count}')
print(f'Total width: {total_w}')

image = Image.new('RGBA', (total_w, letter_h))

x = 0

def change_img_color(img: Image, color = (255, 255, 255, 255)):
    w, h = img.size
    old_pixel_map = img.load()
    new_image = Image.new('RGBA', (w, h))
    new_pixel_map = new_image.load()
    for x in range(w):
        for y in range(h):
            pixel = old_pixel_map[x, y]
            if (pixel == (0,0,0)):
                new_pixel_map[x, y] = (0,0,0,0)
            else:
                new_pixel_map[x, y] = color
            print (f'Pixel {new_pixel_map[x, y]}')
            # Copy the original pixel to the new pixel map.
            #new_pixel_map[x, y] = pixel

    return new_image

for i, c in enumerate(chars):
    asciival = ord(c)
    path = os.path.join(f'{prefix}', f'{asciival}.png')

    image.putpixel((x,0), pink)
    x += 1

    if os.path.exists(path):
        glyph = Image.open(path)
        #glyph = change_img_color(glyph)
        w, h = glyph.size
        if asciival > 96 and asciival < 123:
            if c in chars_pinbottom:
                offset = (x, letter_h - h)
            else:
                offset = (x, letter_h - h - 1)
        else:
            offset = (x, 1)
        image.paste(glyph, offset)
        x += w + 1
    else: # empty space
       x += 1
    image.putpixel((x,0), pink)

image.save(os.path.join(prefix, 'font.png'))
image.show()

