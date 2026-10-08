# VirtuaNES for 3DS

VirtuaNES is a high compatibility NES emulator for your old 3DS or 2DS. It's not as accurate as FCEUX or Nestopia, but it runs at full 60 FPS for almost all games, and supports tonnes of mappers: MMC1,3,5,6; VRC1,2,3,4,6,7; and tonnes of other mappers. As a result, the library of games it supports are huge.

This 3DS version also fixes a few bugs from VirtuaNES's MMC5 mappers and even plays Rockman 4 Minus Infinity and Zelda Legend of Link hacks.

This is a fork of bubble2k16's [emus3ds](https://github.com/bubble2k16/emus3ds), narrowed down to the VirtuaNES core. See [readme-virtuanes.md](readme-virtuanes.md) for installation, usage and the change history.

While you browse the ROM list, the top screen can show box art and a screenshot of the highlighted game. NES boxes, landscape Famicom covers and square Famicom Disk System covers all fit; [readme-virtuanes.md](readme-virtuanes.md#box-art-and-screenshot-previews) explains where the images go and how `tools/make_previews.py` resizes them.

![alt tag](https://github.com/bubble2k16/emus3ds/blob/master/screenshots/VirtuaNES%20-%20Gradius%20II.bmp)

![alt tag](https://github.com/bubble2k16/emus3ds/blob/master/screenshots/VirtuaNES%20-%20Kirby's%20Adventure.bmp)

## Building

The code builds with a current devkitARM (libctru 2.x). If you have devkitPro installed, run `make`, which produces `virtuanes_3ds.3dsx` and `virtuanes_3ds.cia`.

Without a local devkitPro, `tools/build.sh` runs the same `make` inside the official `devkitpro/devkitarm` Docker image and passes its arguments through, so `tools/build.sh clean` works too.

`make DEBUGOUT=1` turns on the `DEBUGOUT()` traces that are sprinkled through the VirtuaNES core and mappers (ROM header details at load time, unhandled mapper writes and so on). They go to `svcOutputDebugString`, which shows up in the emulator's log. The flag isn't tracked by the build, so run `make clean` when switching it on or off.

## Running and debugging in Azahar

The [Azahar](https://azahar-emu.org/) 3DS emulator (the successor of Citra and Lime3DS) runs the `.3dsx` directly, which is the quickest way to try a change. If you have it installed, open `virtuanes_3ds.3dsx` from it, and put your ROMs on its emulated SD card, the `sdmc` folder in Azahar's user directory. Under an emulator VirtuaNES runs without sound, because Azahar doesn't implement the CSND sound service it uses.

`tools/azahar/run.sh` runs Azahar from the `linuxserver/azahar` Docker image instead, and can script it. By default it runs headless on a virtual display, presses 3DS buttons and takes screenshots of both screens at native size, which is handy for checking that a game boots and looks right after a mapper change:

```sh
tools/build.sh
tools/azahar/run.sh -r game.nes -- wait:8 key:a wait:5 shot:title key:start wait:3 shot:ingame
```

The ROMs given with `-r` are the only ones on the emulated SD card, so pressing A at the ROM menu loads the first one. Screenshots and the Azahar log (including `DEBUGOUT()` output) end up in `.azahar/out`. `tools/azahar/run.sh --help` lists all the steps and options, and `--gui` shows the emulator on your X11 display instead.

For a debugger, start the emulator with `-g`, which makes Azahar's GDB stub wait for a connection, and then attach devkitARM's GDB with `tools/azahar/gdb.sh`:

```sh
tools/azahar/run.sh -r game.nes -g -t 3600 &
tools/azahar/gdb.sh
(gdb) break Mapper004::Write
(gdb) continue
```

`tools/azahar/smoketest.py` checks the whole setup end to end. It builds a tiny test ROM that shows a blue screen and turns it red while A is held, runs it in Azahar and checks the screenshots.

`tools/azahar/mappertest.py` does the same for mappers: it generates a test ROM per mapper whose code sets the mapper's registers and checks what the CPU and PPU then see (every PRG and CHR bank starts with its own number) and how the nametables are mirrored. The screen turns green when every check passes, and red otherwise. Pass mapper numbers to run only those, e.g. `tools/azahar/mappertest.py 210`.

## Adding a mapper

Mappers live in `src/cores/virtuanes/NES/Mapper`, one class per mapper (`MapperNNN.h` and `MapperNNN.cpp`) deriving from `Mapper`. They are compiled as part of `src/cores/virtuanes/NES/MapperFactory.cpp`, which `#include`s every mapper header and source file, and whose `CreateMapper()` maps iNES mapper numbers (and UNIF board names) to the classes. So a new mapper needs its two files plus the two `#include`s and a `case` in `CreateMapper()`. The existing mappers such as `Mapper003` (CNROM) are the best reference for the bank switching helpers (`SetPROM_8K_Bank()`, `SetVROM_1K_Bank()` and friends).

To test a new mapper, add a test function and a `TESTS` entry to `tools/azahar/mappertest.py`. [docs/mappers.md](docs/mappers.md) lists the iNES mappers that VirtuaNES doesn't support yet, and which ones are the easiest to add.

## Mappers still to do

VirtuaNES handles 197 of the 256 iNES 1.0 mapper numbers. These 45 have known hardware and still need implementing. The examples come from each mapper's page on the [NESdev wiki](https://www.nesdev.org/wiki/Mapper), and [docs/mappers.md](docs/mappers.md) has the board names and notes.

| Mapper | Kind | Example games |
|---:|---|---|
| 14 | Other | Samurai Spirits (Rex Soft pirate) |
| 29 | Homebrew | Glider |
| 31 | Homebrew | NSF-style music compilations: 2A03 Puritans, RNDM |
| 38 | Other | Crime Busters |
| 53 | Multicart | Supervision 16-in-1 |
| 54 | Multicart | Novel Diamond 9999999-in-1 |
| 55 | Pirate conversion | Fly Merio Bros., Super Mario Bros. Malee 2 |
| 56 | Pirate conversion | Super Mario Bros. 3 (pirate reproduction) |
| 59 | Multicart | None named (BMC-T3H53 and BMC-D1038 boards) |
| 63 | Multicart | Powerful 250-in-1, Hello Kitty 255-in-1 |
| 81 | Other | Super Gun (NTDEC) |
| 103 | Pirate conversion | Doki Doki Panic (FDS conversion) |
| 104 | Other | Pegasus 5-in-1: Big Nose Freaks Out, Micro Machines, Fantastic Adventures of Dizzy |
| 106 | Pirate conversion | Super Mario Bros. 3 (bootleg) |
| 123 | MMC3 variant | Mortal Kombat 3, Earthworm Jim 2 (pirate) |
| 124 | Other | Super Game Mega Type III (pirate arcade board) |
| 125 | Pirate conversion | Monty no Doki Doki Daisassou (FDS conversion) |
| 126 | MMC3 multicart | Power Joy Classic TV Game 84-in-1, Gamezone 118-in-1 |
| 127 | Other | Double Dragon II (pirate) |
| 128 | Other | T-262 multicarts |
| 136 | Sachen | 四川麻將 (Sichuan Mahjong), 未来小子 |
| 137 | Sachen | The Great Wall |
| 138 | Sachen | None named |
| 139 | Sachen | None named |
| 143 | Sachen | Dancing Blocks, Magical Mathematics |
| 144 | Other | Death Race |
| 145 | Sachen | Sidewinder |
| 147 | Sachen | Challenge of the Dragon |
| 149 | Sachen | Taiwan Mahjong 16 |
| 175 | Other | Kaiser 15-in-1 |
| 186 | Other | Fukutake Study Box (BIOS) |
| 196 | MMC3 variant | MRCM's Mario-themed hacks |
| 197 | MMC3 variant | Super Fighter III, Mortal Kombat III Special |
| 203 | Multicart | 35-in-1 |
| 204 | Multicart | 64-in-1, 80-in-1 |
| 205 | MMC3 multicart | 15-in-1, 3-in-1 |
| 208 | MMC3 variant | Street Fighter IV (pirate) |
| 214 | Multicart | Super Gun 20-in-1 |
| 215 | MMC3 variant | Mortal Kombat 3, Earthworm Jim 2, Pocahontas Part 2 (pirate) |
| 217 | MMC3 multicart | 500-in-1, 2000-in-1 |
| 218 | Homebrew | Magic Floor, Starfight |
| 219 | MMC3 variant | Toy Story, Super 1997 4-in-1 |
| 221 | Multicart | None named (NTDEC N625092 boards) |
| 238 | MMC3 variant | Contra Fighter |
| 250 | MMC3 variant | Time Diver Avenger, Queen Bee V |

The other 14 unhandled numbers (39, 84, 98, 102, 129, 130, 131, 146, 161, 213, 223, 224, 239 and 247) are bad or unused assignments, and a ROM using one is best fixed with the right number in its header.

Mappers above 255 come from the NES 2.0 header format, which VirtuaNES doesn't read yet. Supporting that header comes before any of them.

### VT03, VT09 and other VRT famiclones

Many plug-and-play consoles and handhelds run on V.R. Technology's NES-compatible chips (VT02, VT03, VT09, VT32, VT369 and others), usually as "OneBus" ROMs under NES 2.0 mapper 256 and its relatives. They're further off than the mappers above: besides the NES 2.0 header, the newer chips add graphics modes with more colours per tile, and some add their own sound hardware, so they need changes to VirtuaNES's PPU and APU as well as a mapper. They aren't planned yet.
