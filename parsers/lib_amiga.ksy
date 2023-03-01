meta:
  id: sr_libp_amiga
  title: Street Rod 'lib.p' Amiga
  endian: be

seq:
  - id: num_sprites
    type: u2
  - id: crap
    size: 2 + num_sprites * 28
  - id: sprites
    type: sprite_descriptor
    repeat: expr
    repeat-expr: num_sprites
  - id: data
    size-eos: true



# (short width, short height, short sizeDecoded, byte type, byte[] dictionary, byte alwaysZero, int offset, short sizeEncoded)
types:
  sprite_descriptor:
    seq:
      - id: width
        type: u2
      - id: height
        type: u2
      - id: size_decoded
        type: u2
      - id: type
        type: u1
        doc: "Varies from 0 up to 100+"
      seq:
        - id: dictionary
          type: str
          encoding: ascii
          size: 16
          terminator: 0
      - id: zero
        type: u1
        doc: "Enum 0-11"
      - id: offset
        type: u2
      - id: size_encoded
        type: u2


