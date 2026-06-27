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
//  Video backend for the Waveshare ESP32-C6-Touch-LCD-1.47.
//
//  The panel is a 172x320 IPS module driven by a JD9853 controller. The JD9853
//  is command-compatible with the ST7789 for the operations DOOM needs (sleep
//  out, pixel format, address window, RAM write), so it is driven through
//  Arduino_GFX's ST7789 driver with a column offset of 34 (the panel is the
//  centre 172 columns of the controller's 240-wide GRAM).
//
//  DOOM renders an 8-bit 320x200 frame in screens[0]. We present it in
//  landscape (320 wide x 172 tall) and stream it to the panel one row at a
//  time through a single line buffer, so we never allocate a full 16-bit
//  framebuffer (the C6 has only 512KB SRAM and no PSRAM).
//
//-----------------------------------------------------------------------------

#if defined(DOOM_ESP32) && defined(ESP32C6_TOUCH_LCD_147)

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

extern "C" {
#include "doomtype.h"
#include "doomdef.h"
#include "i_video.h"
#include "v_video.h"
}

// DOOM's internal rendering resolution.
#define SRC_W 320
#define SRC_H 200

// Display (landscape) resolution. Defined in platformio.ini.
#ifndef DOOMGENERIC_RESX
#define DOOMGENERIC_RESX 320
#endif
#ifndef DOOMGENERIC_RESY
#define DOOMGENERIC_RESY 172
#endif

// JD9853 panel geometry (portrait native) and SPI pins. From platformio.ini.
#ifndef LCD_PANEL_W
#define LCD_PANEL_W 172
#endif
#ifndef LCD_PANEL_H
#define LCD_PANEL_H 320
#endif
#ifndef LCD_COL_OFFSET
#define LCD_COL_OFFSET 34
#endif
#ifndef LCD_ROW_OFFSET
#define LCD_ROW_OFFSET 0
#endif
#ifndef LCD_SPI_HZ
#define LCD_SPI_HZ 40000000
#endif

static Arduino_DataBus* bus = nullptr;
static Arduino_GFX* gfx = nullptr;

// 256-entry RGB565 palette (native order; Arduino_GFX handles byte order).
static uint16_t palette_rgb565[256];

// One row of output pixels, sized to the widest landscape dimension.
static uint16_t* linebuf = nullptr;

// Actual display dimensions reported by the driver after rotation.
static int dispW = DOOMGENERIC_RESX;
static int dispH = DOOMGENERIC_RESY;

void I_InitGraphics(void)
{
    // Hardware SPI bus. Arduino_ESP32SPI is unreliable on the C6, so use the
    // generic HW SPI databus with explicit pins. The panel itself is
    // write-only, but MISO is kept assigned (GPIO3) so the shared bus stays
    // usable for microSD reads during gameplay.
    bus = new Arduino_HWSPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI, LCD_MISO);

    // Rotation 1 == landscape. The 34px controller offset is a column offset in
    // portrait; under a 90-degree rotation it becomes a row offset, so the two
    // offset pairs are swapped accordingly.
    gfx = new Arduino_ST7789(
        bus, LCD_RST, 1 /* rotation: landscape */, true /* IPS */,
        LCD_PANEL_W, LCD_PANEL_H,
        LCD_COL_OFFSET, LCD_ROW_OFFSET,   // offsets for rotations 0/2
        LCD_ROW_OFFSET, LCD_COL_OFFSET);  // offsets for rotations 1/3

    if (!gfx->begin(LCD_SPI_HZ)) {
        Serial.println("JD9853/ST7789 init failed!");
    }
    gfx->fillScreen(BLACK);

    // Backlight on.
#ifdef LCD_BL
    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, HIGH);
#endif

    dispW = gfx->width();
    dispH = gfx->height();

    linebuf = (uint16_t*)malloc(dispW * sizeof(uint16_t));
    if (linebuf == nullptr) {
        Serial.println("Failed to allocate line buffer!");
    }

    Serial.printf("Display ready: %dx%d (panel %dx%d)\n",
                  dispW, dispH, LCD_PANEL_W, LCD_PANEL_H);
}

void I_ShutdownGraphics(void)
{
    if (linebuf != nullptr) {
        free(linebuf);
        linebuf = nullptr;
    }
#ifdef LCD_BL
    digitalWrite(LCD_BL, LOW);
#endif
}

void I_SetPalette(byte* palette)
{
    // DOOM RGB888 -> RGB565. No manual byte swap: Arduino_GFX sends 16-bit
    // colours big-endian over SPI itself.
    for (int i = 0; i < 256; i++) {
        byte r = palette[i * 3 + 0];
        byte g = palette[i * 3 + 1];
        byte b = palette[i * 3 + 2];
        palette_rgb565[i] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }
}

void I_UpdateNoBlit(void)
{
    // Not used.
}

void I_FinishUpdate(void)
{
    if (gfx == nullptr || linebuf == nullptr || screens[0] == nullptr) {
        return;
    }

    // Nearest-neighbour scale 320x200 -> dispW x dispH, streamed row by row.
    // Precompute the source-column lookup once per frame.
    static int colmap[1024];
    if (dispW <= (int)(sizeof(colmap) / sizeof(colmap[0]))) {
        for (int x = 0; x < dispW; x++) {
            int sx = (x * SRC_W) / dispW;
            if (sx >= SRC_W) sx = SRC_W - 1;
            colmap[x] = sx;
        }
    }

    gfx->startWrite();
    gfx->writeAddrWindow(0, 0, dispW, dispH);
    for (int y = 0; y < dispH; y++) {
        int sy = (y * SRC_H) / dispH;
        if (sy >= SRC_H) sy = SRC_H - 1;
        const byte* srcrow = screens[0] + sy * SRC_W;
        for (int x = 0; x < dispW; x++) {
            linebuf[x] = palette_rgb565[srcrow[colmap[x]]];
        }
        gfx->writePixels(linebuf, dispW);
    }
    gfx->endWrite();
}

void I_WaitVBL(int count)
{
    delay(count * 17);  // ~60Hz frames
}

void I_ReadScreen(byte* scr)
{
    memcpy(scr, screens[0], SRC_W * SRC_H);
}

void I_BeginRead(void)  {}
void I_EndRead(void)    {}

#endif // DOOM_ESP32 && ESP32C6_TOUCH_LCD_147
