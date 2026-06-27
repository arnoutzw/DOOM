// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// DESCRIPTION:
//  ESP32 T-Display video implementation using TFT_eSPI
//
//  Used by the LilyGo T-Display / T-Display S3 targets. The Waveshare
//  ESP32-C6-Touch-LCD-1.47 has its own JD9853 backend (i_video_c6.cpp) and is
//  excluded here.
//
//-----------------------------------------------------------------------------

#if defined(DOOM_ESP32) && !defined(ESP32C6_TOUCH_LCD_147)

#include <Arduino.h>
#include <TFT_eSPI.h>

extern "C" {
#include "doomtype.h"
#include "doomdef.h"
#include "i_video.h"
#include "v_video.h"
}

// Display dimensions - defined in platformio.ini
#ifndef DOOMGENERIC_RESX
#define DOOMGENERIC_RESX 135
#endif
#ifndef DOOMGENERIC_RESY
#define DOOMGENERIC_RESY 240
#endif

// DOOM's internal resolution
#define SCREENWIDTH  320
#define SCREENHEIGHT 200

static TFT_eSPI tft = TFT_eSPI();

// Color palette lookup table (256 entries, RGB565 format)
static uint16_t palette_rgb565[256];

// Frame buffer for scaled output
static uint16_t* framebuffer = nullptr;

// Screen buffer pointer from DOOM
extern byte* screens[5];

void I_InitGraphics(void)
{
    // Initialize display
    tft.init();
    tft.setRotation(1);  // Landscape mode
    tft.fillScreen(TFT_BLACK);

    // Enable backlight
#ifdef TFT_BL
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
#endif

    // Allocate framebuffer in PSRAM if available
#ifdef BOARD_HAS_PSRAM
    framebuffer = (uint16_t*)ps_malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * sizeof(uint16_t));
#else
    framebuffer = (uint16_t*)malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * sizeof(uint16_t));
#endif

    if (framebuffer == nullptr) {
        Serial.println("Failed to allocate framebuffer!");
    }
}

void I_ShutdownGraphics(void)
{
    if (framebuffer != nullptr) {
        free(framebuffer);
        framebuffer = nullptr;
    }

#ifdef TFT_BL
    digitalWrite(TFT_BL, LOW);
#endif
}

void I_SetPalette(byte* palette)
{
    // Convert DOOM's RGB888 palette to RGB565 for TFT display
    for (int i = 0; i < 256; i++) {
        byte r = palette[i * 3 + 0];
        byte g = palette[i * 3 + 1];
        byte b = palette[i * 3 + 2];

        // Convert to RGB565 with byte swap for TFT_eSPI
        uint16_t color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        palette_rgb565[i] = (color >> 8) | (color << 8);  // Byte swap for SPI
    }
}

void I_UpdateNoBlit(void)
{
    // Not used in this implementation
}

void I_FinishUpdate(void)
{
    if (framebuffer == nullptr || screens[0] == nullptr) {
        return;
    }

    // Scale DOOM's 320x200 to display resolution
    // Using simple nearest-neighbor scaling
    float scaleX = (float)SCREENWIDTH / DOOMGENERIC_RESX;
    float scaleY = (float)SCREENHEIGHT / DOOMGENERIC_RESY;

    byte* src = screens[0];
    uint16_t* dst = framebuffer;

    for (int y = 0; y < DOOMGENERIC_RESY; y++) {
        int srcY = (int)(y * scaleY);
        if (srcY >= SCREENHEIGHT) srcY = SCREENHEIGHT - 1;

        for (int x = 0; x < DOOMGENERIC_RESX; x++) {
            int srcX = (int)(x * scaleX);
            if (srcX >= SCREENWIDTH) srcX = SCREENWIDTH - 1;

            byte palIndex = src[srcY * SCREENWIDTH + srcX];
            *dst++ = palette_rgb565[palIndex];
        }
    }

    // Push framebuffer to display
    tft.pushImage(0, 0, DOOMGENERIC_RESX, DOOMGENERIC_RESY, framebuffer);
}

void I_WaitVBL(int count)
{
    // Approximate VBL wait (60Hz = ~16.67ms per frame)
    delay(count * 17);
}

void I_ReadScreen(byte* scr)
{
    memcpy(scr, screens[0], SCREENWIDTH * SCREENHEIGHT);
}

void I_BeginRead(void)
{
    // Not used
}

void I_EndRead(void)
{
    // Not used
}

#endif // DOOM_ESP32 && !ESP32C6_TOUCH_LCD_147
