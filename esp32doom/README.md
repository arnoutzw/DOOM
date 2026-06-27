# DOOM for ESP32 Displays

A port of the original 1997 id Software DOOM source (`linuxdoom-1.10`) to
ESP32-based boards with integrated displays. The classic engine is compiled
directly; the Linux/X11-specific modules (`i_video.c`, `i_system.c`,
`i_sound.c`, `i_net.c`, `i_main.c`) are replaced with ESP32 glue in
`esp32doom/src`.

## Supported Boards

| Target env | Board | MCU | Display | Input |
|------------|-------|-----|---------|-------|
| `lilygo-t-display` | LilyGo T-Display | ESP32 | ST7789 135×240 (TFT_eSPI) | 2 buttons |
| `lilygo-t-display-s3` | LilyGo T-Display S3 | ESP32-S3 | ST7789 170×320 (TFT_eSPI) | 2 buttons |
| `esp32-c6-touch-lcd-147` | **Waveshare ESP32-C6-Touch-LCD-1.47** | ESP32-C6 | **JD9853 172×320 (Arduino_GFX)** | **AXS5106L capacitive touch** |

## ⚠️ Memory reality on the ESP32-C6 (read this first)

The ESP32-C6 has **512 KB of SRAM and no PSRAM**. The classic DOOM engine
allocates four 320×200 screen buffers (~250 KB) **plus** a multi-megabyte zone
heap. This port aggressively shrinks that footprint (see *Fitting in 512 KB*
below), but the classic engine's working set still sits at the very edge of —
and for a full level, beyond — the C6's physically available RAM.

**What is verified:** the firmware **compiles and links** into a flashable
image (toolchain set up in this repo's CI notes), and the static-RAM diet
described below is measured from the linked ELF.

**What needs the actual board:** runtime behaviour (boot, title screen, touch
menus) has not been validated on hardware in this environment. The math says
the title/menu are borderline-feasible and a full level is not — see the
budget below.

The LilyGo T-Display **S3** (8 MB PSRAM) remains the only comfortable target
for actual gameplay.

## Fitting in 512 KB RAM

The ESP32-C6 has **512 KB SRAM, no PSRAM**. After the Arduino/IDF system claims
its share, the linker leaves an `sram_seg` of **~441 KB** for the application
(static data **and** heap). Measured from the linked firmware:

| Region | Size |
|--------|------|
| `sram_seg` total (static + heap) | ~441 KB |
| App + IDF static (`.data`+`.bss`, up to `_heap_start`) | ~229 KB |
| → of which mandatory IDF/WiFi/**BLE** system static | ~54 KB |
| **Free heap at boot** | **~212 KB** |
| …minus FreeRTOS/loop + DOOM task stacks (~40 KB) | **~170 KB usable** |

DOOM's unavoidable runtime allocations: WAD directory (~55 KB for shareware's
2306 lumps) + one 320×200 framebuffer (64 KB) = **119 KB fixed**, leaving only
~50 KB for the zone heap — enough to reach the title/menu, **not** a level
(E1M1 alone needs ~142 KB of resident level geometry, measured from the WAD).

### RAM optimizations applied (static RAM 348 KB → 180 KB by PlatformIO's metric)

- **Read-only math tables → flash.** `finesine`/`finetangent`/`tantoangle`
  (~65 KB) are made `const` so they live in XIP flash instead of DRAM.
- **State table → flash.** `info.c`'s `states[]` (~27 KB), never written at
  runtime, is forced into `.rodata` via a section attribute.
- **Renderer scratch trimmed.** `MAXVISPLANES` 128→64, `MAXOPENINGS` 64→24×width,
  `MAXVISSPRITES`/`MAXDRAWSEGS` reduced — frees ~75 KB of `.bss`. (Raise these
  if you hit *"no more visplanes"* / opening overflow.)
- **Framebuffers: 4 → 1.** The screen wipe is disabled and `DOOM_SCREEN_BUFFERS`
  defaults to 1 on this board (64 KB instead of 256 KB). Border/intermission
  paths then scribble the visible buffer (cosmetic) rather than allocate more.
- **Tunable zone.** `DOOM_ZONE_SIZE` sets the heap budget for the zone.

All knobs are `-D` flags in `platformio.ini`; tune them to your board's measured
`ESP.getFreeHeap()`.

### The honest conclusion

Even fully optimized, WAD-directory + framebuffer + a level's geometry exceed
the ~170 KB usable heap. Running an actual level on this PSRAM-less board would
require deeper engine surgery (memory-mapping WAD lumps from flash to avoid the
zone cache, compacting `lumpinfo`, and a custom IDF build that drops the ~54 KB
of BLE/WiFi static). The framebuffer + title/menu path is the realistic ceiling
here; gameplay wants PSRAM.

## Building

PlatformIO is required. The ESP32-C6 target uses the community
[pioarduino](https://github.com/pioarduino/platform-espressif32) platform
(the upstream `espressif32` platform historically lagged on the C6). The
release tag is pinned in `platformio.ini`; bump it if you need a newer core.

```bash
# Waveshare ESP32-C6-Touch-LCD-1.47
pio run -e esp32-c6-touch-lcd-147

# Flash it
pio run -e esp32-c6-touch-lcd-147 --target upload

# Serial monitor (USB CDC)
pio device monitor -b 115200

# LilyGo targets
pio run -e lilygo-t-display
pio run -e lilygo-t-display-s3
```

## WAD File Setup (ESP32-C6)

The engine looks for an IWAD (`doom1.wad`, `doom.wad`, `doom2.wad`, …) in the
mounted storage directory. On boot the firmware mounts, in order of preference:

1. **microSD card (FAT)** — recommended. Copy `doom1.wad` to the **root** of a
   FAT32-formatted card and insert it. The card shares the display SPI bus
   (CS = GPIO4).
2. **Internal LittleFS** — fallback. The 8 MB partition table
   (`esp32doom/partitions_8mb.csv`) reserves a ~4.9 MB `storage` partition.
   Upload `doom1.wad` with:
   ```bash
   mkdir -p data && cp /path/to/doom1.wad data/
   pio run -e esp32-c6-touch-lcd-147 --target uploadfs
   ```
   (Note: a 4 MB WAD is a tight fit alongside a 3 MB app; the SD card is the
   safer choice.)

The detected directory is exported via `setenv("DOOMWADDIR", …)`, with a
compile-time fallback of `/sd` (`-DDOOM_WADDIR`).

## Controls

### Touch (ESP32-C6-Touch-LCD-1.47)

The panel is **single-touch**, used in landscape, split into a 3×2 grid. The
mapping is **context-sensitive**:

**In a level:**
```
+------------+------------+------------+
| STRAFE L   |  FORWARD   |    FIRE    |
+------------+------------+------------+
|  TURN L    |  BACKWARD  |  USE/OPEN  |
+------------+------------+------------+
```

**In menus / intermission:**
```
+------------+------------+------------+
|    ESC     |    UP      |   ENTER    |
+------------+------------+------------+
|    ESC     |   DOWN     |   ENTER    |
+------------+------------+------------+
```

Because only one finger is tracked, you move *or* turn *or* fire at a time. If
the touch axes feel inverted on your unit, flip `TOUCH_INVERT_X` /
`TOUCH_INVERT_Y` in `platformio.ini`.

### Buttons (LilyGo T-Display boards)

| Button | Function |
|--------|----------|
| Button 1 | Fire (`KEY_RCTRL`) |
| Button 2 | Use (space) |

## Hardware Pin Map (ESP32-C6-Touch-LCD-1.47)

| Function | GPIO |
|----------|------|
| LCD SCK | 1 |
| LCD MOSI | 2 |
| LCD MISO (SD reads) | 3 |
| LCD CS | 14 |
| LCD DC | 15 |
| LCD RST | 22 |
| LCD Backlight | 23 |
| Touch SDA | 18 |
| Touch SCL | 19 |
| Touch RST | 20 |
| Touch INT | 21 |
| microSD CS | 4 |

The JD9853 is driven via Arduino_GFX's `Arduino_ST7789` driver (the command set
DOOM needs is compatible) with a **column offset of 34** — the 172-pixel panel
is the centre of the controller's 240-pixel-wide GRAM.

## Source Layout

```
esp32doom/
├── partitions_8mb.csv     # 8 MB flash layout (3 MB app + ~4.9 MB storage)
└── src/
    ├── main.cpp           # Arduino entry point, FS mount, DOOM task
    ├── i_video_esp32.cpp  # TFT_eSPI video (LilyGo boards)
    ├── i_video_c6.cpp     # JD9853 video via Arduino_GFX (C6 board)
    ├── i_system_esp32.cpp # Timing, zone memory, input, I_Error
    ├── i_sound_esp32.cpp  # Silent sound/music stubs
    ├── i_net_esp32.cpp    # Single-player network stub
    ├── i_touch.cpp        # AXS5106L capacitive touch driver
    └── i_touch.h
```

## Performance Notes

- DOOM's native 320×200 frame is nearest-neighbour scaled to the panel and
  streamed one row at a time (no full 16-bit framebuffer) to conserve RAM.
- Sound is not implemented (silent build).
- Frame rate depends heavily on scene complexity and SPI clock.

## License

Based on the original DOOM source code released by id Software. See
`LICENSE.TXT` in the repository root.
