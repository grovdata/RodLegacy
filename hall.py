from construct import *

fp_amiga = open('hall_dat','rb')
fp_dos = open('dos/SR1/hall_dat','rb')
magic = 0x80

def parse_xor(b, endian='little'):
    xord = bytes([*map(lambda x: x ^ magic, b)])
    return int.from_bytes(xord, byteorder=endian)

def parse_string(b):
    xord = bytes([*map(lambda x: x ^ magic, b)])
    s = str(xord, 'ascii')
    return s


hall_record = Struct(
    "count" / Short,
    "rows" / Array(this.count,
        Struct(
        "nameBytes" / Array(16, Byte),
        "name" / Computed(lambda this: parse_string(this.nameBytes)),
        "scoreBytes" / Array(7, Byte),
        "score" / Computed(lambda this: parse_xor(this.scoreBytes))
        )
    )
)

hall_record_amiga = Struct(
    "count" / Short,
    "rows" / Array(this.count,
        Struct(
        "nameBytes" / Array(16, Byte),
        "name" / Computed(lambda this: parse_string(this.nameBytes)),
        "scoreBytes" / Array(7, Byte),
        "score" / Computed(lambda this: parse_xor(this.scoreBytes, endian='little'))
        )
    )
)

chunk = fp_dos.read()
d = hall_record.parse(chunk)
for row in d['rows']:
    print (f'{row["name"]}: {row["score"]}')
#print(d)
#print (len(d))

#row1 = d['rows'][0]['name']

#test = bytes([*map(lambda x: x ^ magic, row1)])
#print(test)
#print(str(test, 'ascii'))

amiga_bytes = fp_amiga.read()
a = hall_record_amiga.parse(amiga_bytes)
print ('Amiga')
for row in a['rows']:
    print (f'{row["name"]}: {row["score"]}')
