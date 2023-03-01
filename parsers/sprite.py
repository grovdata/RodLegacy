from sr_libp_amiga import *
import sys
ks = KaitaiStream(BytesIO(open(sys.argv[1],'rb').read()))
#print(ks)

lib = SrLibpAmiga(ks)
size_all = 0
types = set()
zeros = set()
for s in lib.sprites:
    size_all += s.size_encoded
    zeros.add(s.zero)
    types.add(s.type)

    print(f'total size: {size_all}, types: {types} zeros: {zeros}')

RLE_COUNT = [ 1, 1, 2, 3, 4, 2, 3, 5, 6, 6, 4, 7, 7, 5, 8, 8 ]
RLE_VALUE = bytes([0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0x00 ])
SPRITE_IDENTIFIER = 0xBC

data = lib.data

for sprite in lib.sprites:


    p = 0
    magic_word = data[p] & 0xFF
    p += 1

    if (magic_word != SPRITE_IDENTIFIER):
        print("unknown compression")

    width = sprite.width / 8
    height = sprite.height
    depth = 5 # ?? guess
    bitmap = [[0 for i in range(int(width * height))] for j in range(depth)]
    #print (bitmap)
    pos = 0

    while(pos < (width * height * depth)):
        b = data[p]
        p += 1
        found = False

        for i, d in enumerate(sprite.directory):
            if d == 0:
                break #end
            #if b == d:
            #    for c in range(0, )

        if found:
            continue

        if b == 0:
            c = data[p] & 0xFF
            p += 1
            pos = c + 1
        elif b == 0xFF:
            c = data[p] & 0xFF
            p += 1
            for i in range(0, c):
                bitmap[int(pos / width)][pos % width] = 0xFF
                pos += 1
        else:
            bitmap[int(pos / width)][pos % width] = b
            pos += 1

#print (bitmap)