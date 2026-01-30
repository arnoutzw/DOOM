// DOOM for ESP32 T-Display
//
// Main entry point for Arduino/PlatformIO build
//
// This is a port of the original DOOM source code to ESP32-based
// devices, specifically targeting the LilyGo T-Display boards.
//
// Requirements:
// - LilyGo T-Display or T-Display S3
// - DOOM WAD file on SD card or SPIFFS
// - PlatformIO for building
//

#ifdef DOOM_ESP32

#include <Arduino.h>

extern "C" {
#include "doomdef.h"
#include "d_main.h"

// Main DOOM entry point
void D_DoomMain(void);
}

// Forward declarations
void doom_task(void* parameter);

void setup()
{
    Serial.begin(115200);
    delay(1000);  // Wait for serial to initialize

    Serial.println();
    Serial.println("=================================");
    Serial.println("  DOOM for ESP32 T-Display");
    Serial.println("=================================");
    Serial.println();

    // Print memory info
    Serial.printf("Total heap: %d bytes\n", ESP.getHeapSize());
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

#ifdef BOARD_HAS_PSRAM
    Serial.printf("Total PSRAM: %d bytes\n", ESP.getPsramSize());
    Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
#endif

    Serial.println();
    Serial.println("Starting DOOM...");

    // Create DOOM task on core 1 with large stack
    xTaskCreatePinnedToCore(
        doom_task,    // Task function
        "DOOM",       // Task name
        32768,        // Stack size (bytes)
        NULL,         // Parameters
        1,            // Priority
        NULL,         // Task handle
        1             // Core ID (1 = app core)
    );
}

void loop()
{
    // Main loop does nothing - DOOM runs in its own task
    delay(1000);
}

void doom_task(void* parameter)
{
    (void)parameter;

    // Set up DOOM command line arguments
    // Note: WAD file path will need to be configured based on storage method
    char* argv[] = {
        (char*)"doom",
        (char*)"-iwad",
        (char*)"/spiffs/doom1.wad",  // Shareware WAD on SPIFFS
        NULL
    };
    int argc = 3;

    // Store arguments for DOOM's myargc/myargv
    extern int myargc;
    extern char** myargv;
    myargc = argc;
    myargv = argv;

    // Start DOOM
    D_DoomMain();

    // Should never reach here
    Serial.println("DOOM exited unexpectedly");
    vTaskDelete(NULL);
}

#endif // DOOM_ESP32
