## Development notes

### MacOS

Add app bundle: https://tmewett.com/making-macos-bundle-info-plist/

## DATA file

Filename DOS: `HOT_DATA`

### Amiga

#### Notable offsets in game binary

| Offset  | What                                  | Size    |
| ------- | ------------------------------------- | ------- |
| 0x29b76 | Possibly start of palette data tables | 64bytes |

Filename Amiga: `SR_data`

"Get lost!"

Watch: w 2 0xC364A0 1 r

000C0CE0E 4a18 TST.B (A0)+
00C0CE10 66fc BNE.B #$fffffffc == $00c0ce0e (T)
00C0CE12 5388                     SUBA.L #$00000001,A0
00C0CE14 91ca                     SUBA.L A2,A0
00C0CE16 2008                     MOVE.L A0,D0
00C0CE18 48c0                     EXT.L D0
00C0CE1A 226d fffc                MOVEA.L (A5, -$0004) == $00c43cf6,A1
00C0CE1E 204a                     MOVEA.L A2,A0
00C0CE20 4eae ffc4                JSR (A6, -$003c) == $00c01de2
00C0CE24 4ced 4cc0 ffe8           MOVEM.L (A5, -$0018) == $00c43ce2,D6-D7/A2-A3/A6
00C0CE2A 4e5d                     UNLK.L A5
00C0CE2C 4e75                     RTS
00C0CE2E 48e7 0012                MOVEM.L A3/A6,-(A7)
00C0CE32 266f 000c                MOVEA.L (A7, $000c) == $00c43cc6,A3
00C0CE36 206c 9534                MOVEA.L (A4, -$6acc) == $00c3744c,A0
00C0CE3A 224b                     MOVEA.L A3,A1
00C0CE3C 4a19                     TST.B (A1)+
00C0CE2E 48e7 0012                MOVEM.L A3/A6,-(A7)
00C0CE32 266f 000c                MOVEA.L (A7, $000c) == $00c43cc6,A3
00C0CE36 206c 9534                MOVEA.L (A4, -$6acc) == $00c3744c,A0
00C0CE3A 224b                     MOVEA.L A3,A1
00C0CE3C 4a19                     TST.B (A1)+
00C0CE3E 66fc                     BNE.B #$fffffffc == $00c0ce3c (T) // "Get lost!"
00C0CE40 5389 SUBA.L #$00000001,A1
00C0CE42 93cb SUBA.L A3,A1
00C0CE44 2009 MOVE.L A1,D0
00C0CE46 48c0 EXT.L D0
00C0CE48 2268 000c MOVEA.L (A0, $000c) == $00c06d1c,A1
00C0CE4C 204b MOVEA.L A3,A0
00C0CE4E 2c6c 9088 MOVEA.L (A4, -$6f78) == $00c36fa0,A6
00C0CE52 4eae ffca JSR (A6, -$0036) == $00c01de8
00C0CE56 4cdf 4800 MOVEM.L (A7)+,A3/A6

# library format

These files contains images for the game.
Big endian.

## numBitmaps

uint16: Number of bitmap entries in file

## Bitmap records

The following record repeats `numBitmaps` times.

### width

Type: uint16
Image width

### height

Type: uint16
Image height

### bitmapType

Type: uint8
Unknown but probably an enum to indicate bit depth or other info about bitmap.
Some bitmaps in DOS version are mask images, this is likely indicated in this bitfield.

### unknown

Type: uint8
Unknown, seems always zero.

### rleDictionary

Type: uint8[16]
Dictionary used for RLE unpacking of bitmap data.

### offset

Type: uint32
Offset for the data portion for this bitmap.
Calculate data start something like: `2 + (numBitmaps * sizeof(Bitmap)) + offset`.

### sizeEncoded

Type: uint16
Size of the bitmap data in packed state (RLE compression).

# libstart.p

| Number | What                         |
| ------ | ---------------------------- |
| 0      | Jukebox copy protection      |
| 1      | California Dreams black logo |
| 2      | CF AD 1990 license plate     |
| 3      | Intro screen                 |
| 4      | Intro screen animation sheet |
| 5      | Intro screen photo border    |

# lib.p

| Number | What                             |
| ------ | -------------------------------- |
| 10     | Newspaper 1                      |
| 11     | Newspaper 2                      |
| 296    | King street rodders (high score) |
| 298    | Lip animation after winning      |
| 318    | DMV                              |
| 340    | The gas station                  |
| 365    | The garage                       |
