# Welcome to JSmooCh

JSMooCh started as C ports of my emulators from JSMoo. It has since then expanded into more emulators.

There is a fairly heavy emphasis on debugging, with cores offering different features as they make sense for a platform.

All the cores are in C99 so far. They should compile anywhere, although endianness is not accounted for. They all pretty much assume little-endian.

jsmooch-gui is the front-end GUI app.
jsmooch-lib is the C11 library of emulators, agnostic to front-end.
jsmooch-tests are some tests for jsmooch-lib

## jsmooch-gui progress
jsmooch-gui is very barebrones. It doesn't contain many QoL features I'd probably enjoy like a great debugger or even the ability to choose the ROM at run-time.

It's mostly there to look pretty sometimes. This is a project mostly for me.

## jsmooch-lib/core progress
The emulator cores, on the other hand, could be used by anybody! They all share a common interface based around describing the phyiscal I/O needed (no hardware-accelerated rendering, yet).

The emulators are not the fastest. The emphasis has been on making them "fast enough for me" while also being very accurate and easy to understand.

"Fast enough" comes into focus for the NDS, which, for now, runs >80FPS on my MacBook Air M2. There are plenty of opportunities to optimize older cores like GBA.

### Apple IIe
Won't boot. Just basic keyboard, ram, bus, etc. support. Needs work to boot.

### Atari 2600
Needs a lot of work, can't really play games. Mostly needs work on the TIA, can't find a good source

### Dreamcast
Loads and plays homebrew, boots BIOS to the point of 3d commands. I never actually implemented the 3d commands, though.

### Galaksija
The Yugoslavian microcomputer from the 80's! Boots and displays properly. Not really tested beyond that, lacks tape support too.

### GameBoy/Color
Works, with sound, pretty well. A few glitches here and there still, and a few incompatible GBC games, but mostly a competent core.

### GameBoy Advance
Works, with sound, pretty well. A fairly competent core with decent accuracy.

### Genesis/MegaDrive
Works, with partial sound, pretty well. I've tried over 70 games and gotten about 97% compatability (about 93% with no glitches whatsoever).

### Mac Classic
Boots to sad Mac. Needs work on floppies.

### NES
Pretty good compatability, with sound. Needs a little more work on sound to get it perfect (mostly Mario jump issues), and also MMC5 isn't great. Other than that games play nearly perfectly.

### Nintendo DS
As of this point, it boots and runs Pokemon Mystery Dungeon Blue, but not perfectly. newer 2D features for NDS are a WIP, 3D is a WIP, sound is a WIP

### Master System/Game Gear
Pretty good compatability, with sound.

### PlayStation 1
Decent GPU, GTE and CPU pass all tests. Runs homebrew fine. No CDROM core yet, no audio.

### Super NES
Not ported from JSMoo yet. But, there, only ran a few games. Mostly a proof of concept, I'd probably start over from scratch if I did SNES in C.

### ZX Spectrum
Mostly plays games fine, but not thoroughly tested

More detailed info on each to come!

## Build status

[![Build Linux](https://github.com/raddad772/jsmooch-emus-win/actions/workflows/build-linux.yml/badge.svg)](https://github.com/raddad772/jsmooch-emus-win/actions/workflows/build-linux.yml)
[![Build Windows](https://github.com/raddad772/jsmooch-emus-win/actions/workflows/build-windows.yml/badge.svg)](https://github.com/raddad772/jsmooch-emus-win/actions/workflows/build-windows.yml)
[![Build macOS](https://github.com/raddad772/jsmooch-emus-win/actions/workflows/build-macos.yml/badge.svg)](https://github.com/raddad772/jsmooch-emus-win/actions/workflows/build-macos.yml)

