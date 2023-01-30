meta:
  id: srdata
  title: Street Rod Amiga hall_dat file
  endian: be
  bit-endian: be
types:
  hall_record:
    seq:
      - id: name
        #type: str
        #encoding: ascii
        size: 16
        # process: xor(0x80) only works with integers :/
      - id: score
        size: 7
        process: xor(0x80)
seq:
  - id: num_entries
    type: u2
  - id: hall
    type: hall_record
    repeat: expr
    repeat-expr: num_entries
