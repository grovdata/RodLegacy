from construct import *

num_drivers = 22
driver_record_size = 18
fp_dos = open('dos/SR1/HALL_DAT','rb')
magic = 0x80

def parse_xor(b):
    xord = bytes([*map(lambda x: x ^ magic, b)])
    return int.from_bytes(xord, byteorder='little')

def parse_string(b):
    xord = bytes([*map(lambda x: x ^ magic, b)])
    s = str(xord, 'ascii')
    return s

hall_record = Struct(
    "count" / Short,
    "rows" / Array(10,
        Struct(
        "nameBytes" / Array(16, Byte),
        "name" / Computed(lambda this: parse_string(this.nameBytes)),
        "scoreBytes" / Array(7, Byte),
        "score" / Computed(lambda this: parse_xor(this.scoreBytes))
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

