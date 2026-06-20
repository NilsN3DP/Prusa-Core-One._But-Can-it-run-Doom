# DOOM on the Prusa CORE One

Can it run Doom? Yes. Classic DOOM (E1M1) runs as a standalone, bare-metal firmware
directly on the **xBuddy board** of the Prusa CORE One, rendered on the printer's
display and controlled with the front knob.

**Status:** playable. The picture is upright and centered (320x240, 4:3), the rotary
encoder and button drive the game, and it runs at roughly 7 fps. The stepper motors play the
music, the on-board buzzer handles the sound effects, and it boots to a fire-effect start
screen. The whole thing is fully reversible, so the official Prusa firmware can be flashed
back at any time.

---

## Disclaimer

This is an experimental hobby project, provided as-is and entirely at your own risk. It replaces
the printer's firmware and runs unofficial, unsigned code on the hardware. I did not observe any
damage or lasting issues during my own testing, but I make no guarantees and accept no
responsibility or liability for any damage, malfunction, data loss, voided warranty, or other
problem that may result from building, flashing, or running it.

Flashing custom firmware requires breaking the printer's appendix and may void your warranty. Make
sure you understand the procedure and how to restore the official Prusa firmware before you start.
This project is not affiliated with, endorsed by, or supported by Prusa Research or id Software.

---

## What this is

A from-scratch standalone firmware for the STM32F427 inside the CORE One that brings the
**GBADoom** engine (a build of Doom squeezed into ~256 KB of RAM) onto the printer board.
The firmware temporarily replaces the printer firmware, runs with no Marlin or RTOS
underneath, and talks to the bare hardware: the display, the rotary encoder, and the
internal flash.

It is not a mod of the Prusa firmware. It is a separate firmware flashed through the
existing Prusa bootloader from a USB stick (`.bbf`).

---

## How it came together

It started as a half-joking aside in a Discord — someone floated the idea of getting Doom
running on a Prusa printer, and I figured: sure, let's try it on my CORE One. The path from there:

**1. Read the hardware out of Prusa's firmware.** The Buddy firmware is open source, so the
xBuddy's specifics were all there: STM32F427, the ILI9488 over SPI, the encoder pins, the
clock tree — no probing required.

**2. Pick an engine that fits.** 256 KB of RAM rules out a normal Doom port. GBADoom was
written for the Game Boy Advance around exactly that constraint, so it was the obvious base.

**3. Bare-metal firmware.** Rather than bolt onto the printer firmware, I wrote a minimal
standalone image that does nothing but Doom — clock, SPI display, encoder, flash; no Marlin,
no RTOS.

**4. Fit the WAD.** GBADoom keeps the WAD memory-mapped, and the only mappable storage is the
2 MB internal flash. The shareware IWAD doesn't fit, so I cut it to a single level (E1M1) and
embedded it.

**5. Package for the bootloader.** The Prusa bootloader takes a `.bbf`, not a raw binary, so I
linked the image at the bootloader's app offset and packed it in that format. Fully reversible
— the stock firmware flashes straight back.

**6. Make it boot.** First try was a black screen. With no debugger attached, I beeped the
buzzer at each init stage to bisect where it died, which turned up a broken time base that
stalled the tic loop forever.

**7. Fix it on real hardware.** After that came swapped red/blue, missing controls, a rotated
and mirrored image, and a low framerate — each sorted one at a time against the actual printer
(usually from a photo and a quick "still wrong"), until Doom came up upright, centered,
correctly colored, and playable on the knob.

The result is a playable game of Doom on the printer's screen, driven by the front knob.

---

## The printer plays along

Getting Doom on the screen was the goal; from there it grew into using the rest of the printer
as the sound system and light show. None of it touches anything dangerous — the hotend and bed
heaters are never driven, the motors only dither a fraction of a millimetre in place, and the
fans stay off unless a door opens.

- **Music on the stepper motors.** X, Y, Z and E play "At Doom's Gate" (the E1M1 theme) as
  four-voice polyphony — melody, harmony and bass spread across the axes. A DDS in the 40 kHz
  SysTick toggles each STEP line at the note frequency while flipping DIR every few steps, so
  the motors sing without the toolhead actually going anywhere. The notes are pulled straight
  out of the original `D_E1M1` music lump by [`tools/extract_music.py`](tools/extract_music.py).
- **Sound effects on the buzzer.** The on-board piezo plays a short tone or sweep for each game
  event — weapons, doors, pickups, pain, monster sight/attack/death — layered over the music.
- **A fan whoosh on doors.** Opening or closing a door briefly spins the part-cooling fan.
- **A boot screen.** Power-up shows an animated Doom fire effect with "PRESS START"; a click of
  the knob energizes the motors and starts the game and the music together.

---

## Target hardware (xBuddy / CORE One)

| Part | Value |
|---|---|
| MCU | STM32F427VI, Cortex-M4F @ 168 MHz |
| RAM | 192 KB SRAM + 64 KB CCMRAM = 256 KB (no external SDRAM) |
| Flash | 2 MB internal |
| Display | ILI9488, RGB666 over SPI6 @ 21 MHz |
| Input | rotary encoder + click (the front knob) |

The full pin/clock/init reference is in [`docs/HARDWARE.md`](docs/HARDWARE.md).

---

## How it works

```
doom.bbf  (USB stick, via the Prusa bootloader)
   |  bootloader jumps to 0x08020200
   v
Bare-metal firmware (no Marlin / RTOS)
   - 168 MHz clock, 40 kHz SysTick (encoder polling + stepper-music/SFX synthesis)
   - SPI6 -> ILI9488   SPI5 -> W25Q   GPIO -> encoder
   +-----------------------------------------------+
   | GBADoom engine (240x160, 8-bit paletted)      |
   |   - WAD memory-mapped in internal flash        |
   |   - renders into a CCM framebuffer             |
   +-----------------------------------------------+
   STM32 backend: framebuffer -> display (transposed,
   scaled, palette -> RGB666), encoder/button -> Doom keys
```

The key idea with only 256 KB of RAM: GBADoom does not keep the WAD in RAM. It accesses it
**memory-mapped** (the way the Game Boy Advance original reads the cartridge). On the STM32
the only memory-mappable storage is the 2 MB internal flash, so a **reduced WAD** is baked
into the firmware itself.

### The reduced WAD (E1M1)

GBADoom's full shareware WAD is 3.84 MB and does not fit in the flash left over after the
engine. The tool [`tools/reduce_doom.py`](tools/reduce_doom.py) trims it down to about
1.35 MB without breaking the engine:

- only the **E1M1** map; no sound, music, demos, menu, title, or intermission
- only the **sprites** E1M1 uses (3 monsters plus weapons, items, effects); entirely
  missing sprites are skipped cleanly by the engine
- **TEXTURE1/PNAMES** rewritten to the 33 textures E1M1 references (67 of 350 patches kept);
  unreferenced textures render as a blank wall instead of crashing
- all **flats** are kept (a missing flat referenced by a sector would hard-crash)

A small patch (`DOOM_AUTOSTART`) boots straight into E1M1.

---

## Problems solved along the way

| Problem | Fix |
|---|---|
| WAD does not fit in RAM/flash | GBADoom (memory-mapped WAD) plus an E1M1-only WAD in internal flash |
| Engine would not build for the M4 | replaced the GBA HAL with an STM32 backend; fixed LTO issues (newlib syscalls) |
| Bootloader rejects a raw `.bin` | link at offset `0x08020200`, set `VTOR`, pack as a signature-free `.bbf` |
| HardFault while parsing the WAD | an unaligned 64-bit load (`LDRD`) in `FindLumpByName` became a `memcmp` |
| Hangs before the first frame | `I_GetTime` used `clock()`/`_times` (which never advanced) -> DWT cycle-counter time base |
| Red and blue swapped | the panel reads pixels as B,G,R, so the byte order was adjusted |
| Image rotated and mirrored | with the MK4's orientation (MADCTL 0xE0) the image came out rotated 90° and mirrored; on the CORE One it renders upright with MADCTL 0x40 plus a transposed, horizontally-flipped blit |
| Black/corrupt screen at 42 MHz | this panel needs **21 MHz** for the pixel data (Prusa's "reduce baudrate" case) |
| Encoder unresponsive/jittery | poll the quadrature at **1 kHz** in SysTick instead of 35 Hz in the game loop |

---

## Building

### Dependencies (not included in this repo)

The build expects two reference repos and a toolchain next to this folder:

```
<workspace>/
  doom-xbuddy/            <- this repo
  Prusa-Firmware-Buddy/   git clone (CMSIS headers + utils/pack_fw.py)
  GBADoom/                git clone https://github.com/doomhack/GBADoom
  toolchain/              arm-none-eabi-gcc 13.2 + ninja
```

Also Python 3 with `ecdsa` (`pip install ecdsa`) for packing the `.bbf`.

### Step 1 - generate the WAD (your own game data required)

The game data is not in this repo. You need your own `doom1.wad` / `doom.wad`:

```bash
python tools/reduce_doom.py <your>.wad build/doom_min.wad   # E1M1, ~1.35 MB
../GBADoom/GbaWadUtil/GbaWadUtil.exe -in build/doom_min.wad -cfile src/doom/wad_data.c
printf '\nconst unsigned int doom_iwad_len = sizeof(doom_iwad);\n' >> src/doom/wad_data.c
python tools/extract_music.py <your>.wad D_E1M1 src/doom/doom_music.h   # stepper-music notes
```

### Step 2 - build and pack

```bash
cmake -S . -B build -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=cmake/arm-toolchain.cmake \
      -DCMAKE_MAKE_PROGRAM=<path>/ninja.exe \
      -DCMAKE_BUILD_TYPE=Release
ninja -C build doom.elf            # -> build/doom.bin (fills about 96.8% of the 2 MB flash)
tools/make_bbf.sh build/doom.bin    # -> build/doom.bbf  (CORE One, unsigned)
```

There are two targets: `doom.elf` (the game) and `bringup.elf` (a hardware test with color
bars, a flash check, and encoder/button readout), which is useful for verifying new hardware.

---

## Flashing (CORE One, over USB)

> Prerequisite: the **appendix must be broken** — this is Prusa's official path for running
> custom firmware. See Prusa's guide:
> [Flashing custom firmware](https://help.prusa3d.com/article/zoiw36imrs-flashing-custom-firmware).
> Details and recovery are in [`docs/FLASHING.md`](docs/FLASHING.md).

1. Copy `build/doom.bbf` to the **root** of a **FAT32** USB stick.
2. Insert it into the CORE One's front port and restart the printer.
3. The bootloader offers to update to firmware "99.0.0"; confirm it.

To go back, just flash the official Prusa `.bbf` over USB; it also restores the GUI
resources in the external flash. Everything is fully reversible, and the unique calibration
data lives on a separate EEPROM that is never touched.

---

## Controls (one knob, one button)

At the boot screen, a click of the knob starts the game. In game:

| Input | Action |
|---|---|
| Turn the knob | Look left / right |
| Hold the knob | Walk forward |
| Tap the knob | Fire and use (open doors) |

---

## Project layout

```
doom-xbuddy/
  src/
    startup_stm32f427.c   vector table + reset handler
    system.c              clock (168 MHz), DWT time base, SysTick hook
    gpio.c / spi.c        GPIO + SPI5 (flash) / SPI6 (display)
    ili9488.c             display driver (RGB666, MADCTL 0x40)
    input.c               encoder quadrature + button
    w25q.c                external flash (used by the bring-up test)
    main.c                bring-up firmware (hardware test)
    doom/
      doom_entry.c        entry point: hardware init -> start screen -> D_DoomMain
      i_backend_stm32.c   STM32 backend (video / input / timing / fire start screen)
      tmc_music.c         4-voice stepper music (X/Y/Z/E over TMC2130)
      fx.c                buzzer sound effects + door-triggered fan
      sound_stub.c        audio stub: forwards sound events to fx.c
      wad_data.c          embedded reduced WAD (generated, not committed)
      doom_music.h        extracted stepper-music notes (generated, not committed)
  linker/                 stm32f427.ld (SWD) / stm32f427_boot.ld (bootloader)
  cmake/arm-toolchain.cmake
  tools/
    reduce_doom.py        builds the minimal E1M1 WAD
    extract_music.py      extracts the stepper-music notes from D_E1M1
    make_bbf.sh           packs .bin -> CORE One .bbf
  docs/  HARDWARE.md, FLASHING.md, PLAN.md
```

---

## Known limitations

- Only **E1M1**, no in-game menu. Reaching the level exit may crash, since the intermission
  was removed.
- Audio is the stepper-motor music plus buzzer effects, not the original sampled sound.
- About 7 fps at 320x240. The panel caps the pixel data at 21 MHz, so full-screen would be
  closer to 5 fps and a smaller image would be smoother.
- A few walls may render blank where textures were trimmed away.

---

## Sources and projects used

This project stands on the shoulders of others.

| Project | What it was used for | Link |
|---|---|---|
| Prusa-Firmware-Buddy | Open-source printer firmware: source of the hardware details (pins, clock, display init), the bootloader, and the `.bbf` packing tool (`utils/pack_fw.py`) | <https://github.com/prusa3d/Prusa-Firmware-Buddy> |
| GBADoom (doomhack) | The low-memory Doom engine used as the base, plus the `GbaWadUtil` WAD converter | <https://github.com/doomhack/GBADoom> |
| PrBoom / PrBoom+ | The Doom engine GBADoom is built on | <https://github.com/coelckers/prboom-plus> |
| DOOM (id Software) | The original game and the released source code | <https://github.com/id-Software/DOOM> |
| Arm GNU Toolchain | The `arm-none-eabi-gcc` compiler | <https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads> |
| CMake and Ninja | Build system | <https://cmake.org> and <https://github.com/ninja-build/ninja> |
| Prusa help: custom firmware | Guide to flashing your own firmware (breaking the appendix) | <https://help.prusa3d.com/article/zoiw36imrs-flashing-custom-firmware> |

### Licensing

- **DOOM** is copyright id Software. The WAD game data belongs to id Software or the
  respective owner and is **not** included in this repo.
- **GBADoom** is GPLv2 (based on PrBoom); it is the engine this builds on.
- **Prusa-Firmware-Buddy** is GPLv3; used as a reference for hardware init, the bootloader,
  and the `.bbf` tooling.
- The new code here (STM32 backend, drivers, build/WAD tooling) is **GPLv2**, compatible
  with the GBADoom base.

This is a personal "can it run Doom" hobby project and is not affiliated with id Software
or Prusa Research.
