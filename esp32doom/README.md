# DOOM for ESP32 T-Display

This is a port of the original DOOM source code to ESP32-based devices, specifically targeting the LilyGo T-Display boards.

## Supported Boards

- **LilyGo T-Display** - ESP32 with ST7789 135x240 TFT display
- **LilyGo T-Display S3** - ESP32-S3 with ST7789 170x320 TFT display

## Requirements

- [PlatformIO](https://platformio.org/) (recommended) or Arduino IDE
- LilyGo T-Display or T-Display S3 board
- DOOM WAD file (shareware `doom1.wad` or registered version)
- USB cable for flashing

## Building

### Using PlatformIO CLI

```bash
# Build for T-Display (ESP32)
pio run -e lilygo-t-display

# Build for T-Display S3 (ESP32-S3)
pio run -e lilygo-t-display-s3

# Upload to connected board
pio run -e lilygo-t-display --target upload
```

### Using PlatformIO IDE

1. Open the project folder in VS Code with PlatformIO extension
2. Select the appropriate environment (lilygo-t-display or lilygo-t-display-s3)
3. Click Build/Upload

## WAD File Setup

The DOOM engine requires a WAD file containing game data. You have several options:

### Option 1: SPIFFS (Internal Flash)

1. Create a `data` folder in the project root
2. Place `doom1.wad` in the `data` folder
3. Upload filesystem: `pio run --target uploadfs`

Note: The shareware WAD (doom1.wad) is about 4MB, which may exceed SPIFFS capacity.

### Option 2: SD Card (if available)

Modify `main.cpp` to load the WAD from SD card:
```cpp
char* argv[] = {
    (char*)"doom",
    (char*)"-iwad",
    (char*)"/sd/doom1.wad",
    NULL
};
```

## Controls

The T-Display has two built-in buttons:

| Button | Function |
|--------|----------|
| Button 1 (GPIO 0) | Fire |
| Button 2 (GPIO 35/14) | Use/Action |

For full gameplay, consider adding:
- External buttons or joystick
- Bluetooth gamepad support
- Touch screen controls (T-Display S3)

## Memory Considerations

- The ESP32 has limited RAM (~320KB)
- PSRAM is highly recommended for playable performance
- The T-Display S3 with PSRAM (8MB) provides the best experience
- Zone memory is set to 4MB with PSRAM, 256KB without

## Performance Notes

- The display is scaled from DOOM's native 320x200 resolution
- Frame rate depends on scene complexity
- Sound is not implemented in this basic port

## Directory Structure

```
esp32doom/
├── src/
│   ├── main.cpp           # Arduino entry point
│   ├── i_video_esp32.cpp  # ESP32 video implementation
│   └── i_system_esp32.cpp # ESP32 system implementation
└── README.md
```

## License

Based on the original DOOM source code released by id Software.
See LICENSE.TXT in the repository root for details.
