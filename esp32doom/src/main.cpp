// DOOM for ESP32 displays
//
// Arduino/PlatformIO entry point. Boots a FreeRTOS task that runs the classic
// DOOM main loop (D_DoomMain). Per-board hardware glue lives in the i_*_esp32 /
// i_video_c6 / i_touch source files.
//
// Targets:
//   - LilyGo T-Display / T-Display S3   (TFT_eSPI, ST7789)
//   - Waveshare ESP32-C6-Touch-LCD-1.47 (Arduino_GFX, JD9853 + AXS5106L touch)
//

#ifdef DOOM_ESP32

#include <Arduino.h>

#ifdef ESP32C6_TOUCH_LCD_147
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <LittleFS.h>
#include <stdlib.h>
#endif

extern "C" {
#include "doomdef.h"
#include "d_main.h"

void D_DoomMain(void);

extern int myargc;
extern char** myargv;
}

// Forward declarations
void doom_task(void* parameter);

#ifndef SD_CS
#define SD_CS -1
#endif

// The directory DOOM searches for the IWAD (doom1.wad / doom.wad / ...). Set at
// runtime to whichever filesystem mounts successfully; the engine also has the
// DOOM_WADDIR compile-time fallback.
static const char* g_waddir = NULL;

#ifdef ESP32C6_TOUCH_LCD_147
// Mount storage for the WAD. The microSD card (FAT) is preferred since the
// shareware WAD is ~4MB; internal LittleFS is a fallback if a copy was flashed
// there. Returns the VFS mount directory, or NULL on failure.
static const char* mount_storage(void)
{
    // The SD card shares the display SPI bus. Initialise that bus on the LCD
    // pins before handing it to the SD library.
#if SD_CS >= 0
    // SD shares the display SPI bus (SCK/MOSI) and uses MISO for reads.
    SPI.begin(LCD_SCK, LCD_MISO, LCD_MOSI, SD_CS);
    if (SD.begin(SD_CS, SPI, 20000000)) {
        if (SD.exists("/doom1.wad") || SD.exists("/doom.wad") ||
            SD.exists("/doom2.wad")) {
            Serial.println("WAD found on microSD (/sd)");
            return "/sd";
        }
        Serial.println("microSD mounted but no WAD found at root");
        return "/sd";  // let DOOM report the specific missing file
    }
    Serial.println("microSD mount failed, trying LittleFS...");
#endif

    if (LittleFS.begin(false)) {
        if (LittleFS.exists("/doom1.wad") || LittleFS.exists("/doom.wad") ||
            LittleFS.exists("/doom2.wad")) {
            Serial.println("WAD found on LittleFS (/littlefs)");
            return "/littlefs";
        }
        Serial.println("LittleFS mounted but no WAD found");
    } else {
        Serial.println("LittleFS mount failed");
    }
    return NULL;
}
#endif // ESP32C6_TOUCH_LCD_147

void setup()
{
    Serial.begin(115200);
    delay(1000);  // Wait for serial / USB CDC to come up

    Serial.println();
    Serial.println("=================================");
#ifdef ESP32C6_TOUCH_LCD_147
    Serial.println("  DOOM - ESP32-C6-Touch-LCD-1.47");
#else
    Serial.println("  DOOM for ESP32");
#endif
    Serial.println("=================================");
    Serial.println();

    Serial.printf("Total heap: %d bytes\n", ESP.getHeapSize());
    Serial.printf("Free heap:  %d bytes\n", ESP.getFreeHeap());

#ifdef BOARD_HAS_PSRAM
    Serial.printf("Total PSRAM: %d bytes\n", ESP.getPsramSize());
    Serial.printf("Free PSRAM:  %d bytes\n", ESP.getFreePsram());
#endif

#ifdef ESP32C6_TOUCH_LCD_147
    g_waddir = mount_storage();
    if (g_waddir) {
        // Point the engine's WAD/config search at the mounted filesystem.
        setenv("DOOMWADDIR", g_waddir, 1);
        setenv("HOME", g_waddir, 1);
    } else {
        Serial.println("WARNING: no WAD storage mounted; DOOM will fail to find an IWAD.");
    }
#endif

    Serial.println();
    Serial.println("Starting DOOM...");

    // DOOM needs a large stack; run it in its own task.
    xTaskCreate(
        doom_task,    // Task function
        "DOOM",       // Task name
        32768,        // Stack size (bytes)
        NULL,         // Parameters
        1,            // Priority
        NULL          // Task handle
    );
}

void loop()
{
    // Everything happens in doom_task.
    delay(1000);
}

void doom_task(void* parameter)
{
    (void)parameter;

    // This 1997 codebase has no -iwad option; the IWAD is located via
    // DOOMWADDIR (set above / DOOM_WADDIR fallback). Pass extra command-line
    // arguments here if desired (e.g. "-warp", "1", "1").
    static char arg0[] = "doom";
    static char* argv[] = { arg0, NULL };

    myargc = 1;
    myargv = argv;

    D_DoomMain();

    // D_DoomMain does not return in normal operation.
    Serial.println("DOOM exited unexpectedly");
    vTaskDelete(NULL);
}

#endif // DOOM_ESP32
