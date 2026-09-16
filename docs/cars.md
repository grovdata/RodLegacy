# Car port notes

Research on the original data: `RodLegacyResearch/docs/formats/sr_data.md`,
`hot_data_dos.md` and `srse_lib2.md`.

## Wheel Y offsets are derived, not stored

We position a car's wheels at `bodyTop + BitmapOffset_t::offsetYTires`. No car
data file supplies that number, because it is not data: the wheel cell is
bottom-aligned to the body sprite.

```
offsetYTires = bodySprite.height - wheelSprite.height   // 31 Amiga, 28 DOS
```

That is all `CarSetWheelY` (`src/car/car_set.cpp`) does now. The two
hand-measured tables it used to carry are gone; they were 0..4 px off, which
showed up as SE cars floating over their shadow. Every front wheel arch in
lib.p, SR1's LIB2 and SRSE's LIB2 is cut for the tire at exactly the derived Y
- derivation, per-car numbers and the script are in
`RodLegacyResearch/docs/formats/sr_data.md`.

The shadow follows the same line: `CarBitmap::UpdatePositions` puts it at
`tiresY + wheelCellHeight - 3`. It used to add a hardcoded 32, the Amiga cell
height, which left it 2 px under the tires of every DOS car.

`CarDebug` still pages through every model and nudges the wheel line with
up/down, which is now a way to check the rule rather than to build a table.
The King's car (spec slot 25 in every set) has no `CarModel` - `CarData` only
builds the 25 that are for sale - so the debug scene synthesises one per set
from the sprite offset table's last row.

## The driver portrait hangs off y_driver's bottom edge

`y_driver` is not a top-left Y, which is why it reads 10 to 15 px below where a
top-anchored portrait ends up. It is the sprite's **bottom** edge relative to
the body top:

```cpp
driverTop = bodyTop + offsets.offsetYDriver - driverSprite.height;
```

Measured across all three car libs: the sill of the side window at the driver's
own columns sits 1 to 3 px above `y_driver` on every closed car, so the
portrait's last row is cut by the door line and his shoulders disappear into
it. The open cars (the Corvettes, the Woodie) read wider only because there is
no window frame to measure.

The old code used one constant (7) for every car, which is roughly the average
of `y_driver - height` and looks fine on the 16 to 18 px portraits. It breaks
on the tall ones: DRIVER_15 (asset 356) is 20 px because it draws a hat above
the same head, so top-anchoring pushed the face below the sill and left the hat
filling the window.

CarDebug still shows a portrait and moves it with I/J/K/L, reporting the nudge
alongside the data's `x_driver` / `y_driver`.

## Car sets (how SRSE is supported)

SRSE is not special-cased. It is one entry in a car-set registry
(`src/car_set.h`), and adding another pack should be a matter of adding a
`CarSetDef` plus, if it is not a DOS-format disk, a loader that fills in
`CarSprites`.

- The Amiga stock set is mandatory and always **first**. That keeps its
  car-spec indices at 0..25 - the range original `.SAV` files store - so those
  saves keep loading whatever else is switched on.
- Optional sets append after it in registry order, into one merged car-spec
  table (`State::mergedCarSpecs`). Cars, classifieds, the newspaper, the roster
  and the racer pool all work off that merged table.
- Enabled sets are stored per id (`car_set_srse=1`), so reordering or adding
  registry entries cannot flip what is on.

Nothing downstream branches on "is this SRSE". A car carries its set index and
its already-resolved sprite numbers, so where a chopped roof or bumper patch
lives is answered by a field read rather than a per-source rule.

### What is built

- **DOS HOT_DATA reader.** `DataParser::ParseCarSpecTableDOS`,
  `ParseCarSpritesDOS` and `SrLibDataReadDataFromBufferDOS`
  (`src/parsers/libp.cpp`). The spec reader repacks the DOS parts bytes into
  the Amiga's config word and hands back `SrCarSpecEntry`, so scoring and
  roster code works on either source unchanged. Covered by
  `tests/test_hotdata_dos.cpp` against both shipped files.
- **Asset loading.** `AssetManager::InitCarSets()` reads each optional set's
  lib and data file from its zip (or a loose directory) beside the other game
  data; `GetCarSetSprite(set, index)` decodes on demand and caches. The DOS
  sprite decode is shared with the sticker path.
- **Render path.** `CarModel` carries `carSet` and a resolved `CarSprites`,
  filled in by whichever loader built it - `AmigaCarSprites()` for lib.p cars,
  the set's data file for a DOS pack. `CarModelCanChopRoof` /
  `CarModelCanStripBumpers` are field reads. Only `LoadCarModelSprite` still
  branches, on `CarSetArt`, to pick which lib to read from.
- **Paint.** DOS car sprites keep their paint in palette entries 6 and 7, the
  same two registers the Amiga uses. Measured across the SRSE bodies, those are
  the only entries that follow the body colour, while 1-4 stay black/grey/white
  for outline, chrome and glass and 10/14 are per-car trim.
  `BitmapParser::GetDOSCarPalette` substitutes steps 2 and 6 of the 9-step
  `StandardCarColor` ramp into them. Those two steps are what make Navy
  reproduce the stock blue almost exactly, which is what pins the choice.
  Variants are cached per palette, like the Amiga ones.
- **Wheels and wheelhouse.** lib.p packs a tire's four rotation cells into one
  1x4 spritesheet (32x31 cells); a DOS set gives each cell its own sprite id
  (40x28) - stock at 28-31, brand at 65-68, racing at 106-109, with the arch at
  223. `CarWheel` in `carbitmap.h` holds whichever backing a car's set uses and
  exposes them as N frames, so the animation code does not branch. Frame size
  comes from the wheel rather than a constant, since the two differ.
- **Transparency.** Type 1 DOS sprites key palette entry 0 (pure red) out as
  background. The alpha is punched directly into the pixels rather than set
  with `SDL_SetColorKey`, because `LoadBitmapFromChunky` produces a surface
  that already has an alpha channel and a colour key is ignored when converting
  one of those to a texture - the key colour rendered as an opaque rectangle
  around the sprite. A car ramp cannot collide with the key: RGB4 components
  scale to at most 240, so a repainted body never hits 255,0,0.
- **Car pool.** `CarData::ReadCarSet()` builds a DOS set's models from its own
  data file. `CarModel::index` is shifted by `carSet * HOT_DATA_SPRITE_BASE`,
  because raw SE body numbers (39..179) overlap lib.p's car assets (179..272) -
  slot 24's body really is 179, the same number as the 1940 Chevrolet.
- **Classifieds.** `State::Init` and `Newspaper::LoadClassifiedsData` both walk
  the merged spec table in the same order with the same skip condition - the
  newspaper buys by list position into `State::classifiedCars`, so the two
  lists have to stay index-aligned.
- **Opponents.** The roster draws opponent cars from every enabled set, and
  `RacerPool` is initialised from the merged table. The pool picks challenger
  cars by `SrCarSpecEntry::ForSale()` rather than an index cap, which drops
  each set's King's car and keeps the rest; over the stock table alone that
  selects exactly the same 25 the old cap did.

Enabled by `car_set_srse=1` in the config file
(`~/Library/Application Support/se.grovdata/rodlegacy/config.txt` on macOS).
Defaults off, so nothing changes until it is set.

### What is missing

- **Choosing sets in game.** The config key above is the only control; there is
  no new-game screen for it. Once we have a save writer, the enabled set ids
  belong in the save so a playthrough keeps its roster. Nothing needs it today:
  there is no save writer, and the original `.SAV` loader only ever produces
  stock indices 0..25, which the Amiga-set-first ordering guarantees still mean
  the same cars.
- **Gas cap.** Still the Amiga lib.p sprite for every car. Body, chop, bumper
  patches, wheelhouse and wheels all come from the car's own set.
- **Scene positioning.** `SrLibQueryGetCarRoofYOffset` indexes a per-scene table
  keyed by lib.p asset and falls back to 96 for unknown cars, so SE cars sit at
  the default height in the gas station and similar scenes rather than a
  per-car one.

Racing views need nothing SRSE-specific: the disk changes only three
racing-view car sprites (249, 250, 251) and shares the rest of the on-track art
with stock.

The `filename` field on `CarModel` and the "vette56" entry in `cars.toml` are a
half-built external-art path: `ReadCarsFromTomlFile` populates `filename` but
nothing downstream reads it, and several offset fields are parsed and then not
assigned. It is not the mechanism SRSE uses.
