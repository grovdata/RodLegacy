# HOT_DATA

# // driver stats
# byte unknown (maybe skill level?)
# byte unknown
# byte unknown
# byte unknown
# short sprite_index picture (sprite_index: the first index in the table AC 04 = 1196d, the actual index is 196d, so you have to substract 1000d from each index)
# short string_offset name (string_offset: offset of the string chunk starts at file offset 0x76F)
# byte preferred car?
# @0x0000

# // car stats
# short price
# byte unknown
# byte next in list/order in news letter?
# byte transmission/tires/brand (1=GM, 2=Ford, 4=Chrysler)
# byte carb/manifold/engine
# short string_offset description
# short sprite_index chassis

# /* something like that:
# struct {
# unsigned int transmission : 2;
# unsigned int tires : 2;
# unsigned int brand : 4;
# } parts;

# struct {
# unsigned int carb : 2;
# unsigned int manifold : 4;
# unsigned int engine : 2;
# } engine;
# */
# @0x018C

# //overlay sprite offsets
# short index car_shape (back of car during race)
# short sprite_index chopped_roof
# short sprite_index air_scoop
# short sprite_index rear_bumper
# short sprite_index front_bumper
# @0x0290

# //sprite positions
# positions tires, chopped roof, stripped bumpers, decals
# byte offset_x_rear_tire
# byte offset_x_front_tire
# byte offset_x_driver
# byte offset_y_driver
# byte offset_x_chopped_roof
# byte offset_x_air_scoop
# byte offset_y_air_scoop
# byte offset_y_rear_bumper
# byte offset_x_front_bumper
# byte offset_y_front_bumper
# byte offset_x_decal
# byte offset_y_decal
# // offset x: starts from left side of chassis sprite
# // offset y: starts from top of chassis sprite
# // offset x rear bumper is always 0
# // offset y chopped roof is always 0

# @0x0394

# //click boxes garage
# // for rear bumper, chopped roof, transmission, engine, front bumper
# byte offset_x
# byte offset_y
# byte width
# byte height

# @0x04CC
# // click boxes gas station
# // for tank cap, rear windshield, front windshield
# byte offset_x
# byte offset_y
# // width and height are hard-coded

# @0x06D4
# CODE: SELECT ALL
# // strings
# @0x0770
# CODE: SELECT ALL

# // padding/garbage after strings chunk
# @0x0D21

# // decal strings
# @0x0EAE
# CODE: SELECT ALL
# 27 55 53 41 20 31 27 00 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E  'USA 1'..................
# 27 45 61 67 6C 65 27 00 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E  'Eagle'..................
# 27 4E 6F 74 20 34 20 73 61 6C 65 27 00 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E  'Not 4 sale'.............
# 27 56 65 74 65 72 61 6E 27 00 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E  'Veteran'................
# 27 48 65 6C 6C 27 00 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E  'Hell'...................
# 27 48 61 77 61 69 69 27 00 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E  'Hawaii'.................
# 27 42 61 6E 61 6E 61 20 43 6C 75 62 27 00 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E  'Banana Club'............
# 27 45 78 70 6C 6F 73 69 76 65 27 00 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E 2E        'Explosive'............
# // unknown
# @0x0F74
# // decal info
# short sprite_index decal
# short padding left
# short padding top
# @0x0FB0

meta:
  id: srdata
  title: Street Rod data Amiga
  endian: be
types:
  driver_record:
    seq:
      - id: unknown1
        type: u1
      - id: unknown2
        type: u1
      - id: unknown3
        type: u1
      - id: unknown4
        type: u1
      - id: sprite_index
        type: u2
      - id: name_offset
        type: u2
      - id: preferred_car
        type: u2
      - id: unknown9
        type: u1
      - id: unknown10
        type: u1
      - id: unknown14
        type: u1

seq:
  - id: num_entries
    type: u2
  - id: driver
    type: driver_record
    repeat: expr
    repeat-expr: num_entries
