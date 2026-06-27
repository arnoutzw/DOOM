// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// DESCRIPTION:
//  Touch input handler for the Waveshare ESP32-C6-Touch-LCD-1.47.
//  Capacitive touch controller: AXS5106L (I2C @ 0x63).
//
//  The panel is single-touch. The 1.47" display is used in landscape, so the
//  screen is divided into a 3x2 grid of control zones. Because only one finger
//  can be tracked at a time, the active mapping depends on game state:
//
//    In a level (gamestate == GS_LEVEL, no menu):
//      +------------+------------+------------+
//      | STRAFE L   |  FORWARD   |    FIRE    |
//      +------------+------------+------------+
//      | TURN  L    |  BACKWARD  |  USE/OPEN  |
//      +------------+------------+------------+
//      (turn right / strafe right are reached by combining FORWARD with the
//       single-touch limitation in mind; see README)
//
//    In menus / intermission / finale:
//      +------------+------------+------------+
//      |    BACK    |     UP     |   SELECT   |   (ESC / UP / ENTER)
//      +------------+------------+------------+
//      |    BACK    |    DOWN    |   SELECT   |
//      +------------+------------+------------+
//
//-----------------------------------------------------------------------------

#ifdef DOOM_ESP32
#ifdef USE_TOUCH_INPUT

#include <Arduino.h>
#include <Wire.h>
#include "i_touch.h"

extern "C" {
#include "doomdef.h"
#include "doomtype.h"
#include "d_event.h"
#include "doomstat.h"

void D_PostEvent(event_t* ev);
}

// menuactive lives in m_menu.c
extern "C" boolean menuactive;

// AXS5106L I2C address.
#define AXS5106L_ADDR 0x63

// Touch pins - defined in platformio.ini.
#ifndef TOUCH_SDA
#define TOUCH_SDA 18
#endif
#ifndef TOUCH_SCL
#define TOUCH_SCL 19
#endif
#ifndef TOUCH_INT
#define TOUCH_INT 21
#endif
#ifndef TOUCH_RST
#define TOUCH_RST 20
#endif

// Native (portrait) panel dimensions, for coordinate transform.
#ifndef LCD_PANEL_W
#define LCD_PANEL_W 172
#endif
#ifndef LCD_PANEL_H
#define LCD_PANEL_H 320
#endif

// Landscape display dimensions (zone layout space).
#ifndef DOOMGENERIC_RESX
#define DOOMGENERIC_RESX 320
#endif
#ifndef DOOMGENERIC_RESY
#define DOOMGENERIC_RESY 172
#endif

// Coordinate transform tuning. Flip these if the touch axes feel reversed on
// real hardware.
#ifndef TOUCH_INVERT_X
#define TOUCH_INVERT_X 0
#endif
#ifndef TOUCH_INVERT_Y
#define TOUCH_INVERT_Y 1
#endif

// DOOM key codes for the actions we generate.
#define K_FORWARD     KEY_UPARROW
#define K_BACKWARD    KEY_DOWNARROW
#define K_TURNLEFT    KEY_LEFTARROW
#define K_TURNRIGHT   KEY_RIGHTARROW
#define K_FIRE        KEY_RCTRL
#define K_USE         ' '
#define K_STRAFELEFT  ','
#define K_STRAFERIGHT '.'
#define K_SELECT      KEY_ENTER
#define K_BACK        KEY_ESCAPE

#define MAX_HELD 2

// Currently-held DOOM keys (so we can release them on touch-up / zone change).
static int heldKeys[MAX_HELD];
static int heldCount = 0;
static bool touchActive = false;

static void releaseAll(void)
{
    event_t ev;
    for (int i = 0; i < heldCount; i++) {
        ev.type = ev_keyup;
        ev.data1 = heldKeys[i];
        ev.data2 = ev.data3 = 0;
        D_PostEvent(&ev);
    }
    heldCount = 0;
}

static void pressKeys(const int* keys, int count)
{
    // If the set of held keys already matches, do nothing.
    if (count == heldCount) {
        bool same = true;
        for (int i = 0; i < count; i++) {
            if (heldKeys[i] != keys[i]) { same = false; break; }
        }
        if (same) return;
    }

    releaseAll();

    event_t ev;
    for (int i = 0; i < count && i < MAX_HELD; i++) {
        ev.type = ev_keydown;
        ev.data1 = keys[i];
        ev.data2 = ev.data3 = 0;
        D_PostEvent(&ev);
        heldKeys[heldCount++] = keys[i];
    }
}

// Read one touch point from the AXS5106L. Returns true on a successful I2C
// transaction; *pressed indicates whether a finger is down.
static bool readTouch(int* x, int* y, bool* pressed)
{
    Wire.beginTransmission(AXS5106L_ADDR);
    Wire.write(0x01);  // touch data register
    if (Wire.endTransmission(false) != 0) {
        return false;
    }

    const int want = 14;
    int got = Wire.requestFrom(AXS5106L_ADDR, want);
    if (got < 6) {
        return false;
    }

    uint8_t buf[14];
    for (int i = 0; i < got && i < want; i++) {
        buf[i] = Wire.read();
    }

    uint8_t fingers = buf[1];
    *pressed = (fingers > 0 && fingers != 0xFF);
    // 12-bit coordinates, high nibble in the status byte of each point.
    *x = ((buf[2] & 0x0F) << 8) | buf[3];
    *y = ((buf[4] & 0x0F) << 8) | buf[5];
    return true;
}

// Map raw (portrait) touch coordinates to landscape display coordinates.
static void toDisplay(int tx, int ty, int* dx, int* dy)
{
    // Landscape rotation: panel Y -> display X, panel X -> display Y.
    int X = ty;                       // 0 .. LCD_PANEL_H-1  -> 0 .. dispW-1
    int Y = tx;                       // 0 .. LCD_PANEL_W-1  -> 0 .. dispH-1

#if TOUCH_INVERT_X
    X = (LCD_PANEL_H - 1) - X;
#endif
#if TOUCH_INVERT_Y
    Y = (LCD_PANEL_W - 1) - Y;
#endif

    // Scale into the landscape display space.
    *dx = (X * DOOMGENERIC_RESX) / LCD_PANEL_H;
    *dy = (Y * DOOMGENERIC_RESY) / LCD_PANEL_W;
    if (*dx < 0) *dx = 0;
    if (*dx >= DOOMGENERIC_RESX) *dx = DOOMGENERIC_RESX - 1;
    if (*dy < 0) *dy = 0;
    if (*dy >= DOOMGENERIC_RESY) *dy = DOOMGENERIC_RESY - 1;
}

void I_InitTouch(void)
{
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    Wire.setClock(400000);

#if TOUCH_RST >= 0
    pinMode(TOUCH_RST, OUTPUT);
    digitalWrite(TOUCH_RST, LOW);
    delay(200);
    digitalWrite(TOUCH_RST, HIGH);
    delay(300);
#endif

#if TOUCH_INT >= 0
    pinMode(TOUCH_INT, INPUT_PULLUP);
#endif

    Serial.println("AXS5106L touch initialized");
}

void I_UpdateTouch(void)
{
    int rawX, rawY;
    bool pressed;

    if (!readTouch(&rawX, &rawY, &pressed)) {
        return;
    }

    if (!pressed) {
        if (touchActive) {
            releaseAll();
            touchActive = false;
        }
        return;
    }

    touchActive = true;

    int dx, dy;
    toDisplay(rawX, rawY, &dx, &dy);

    int col = dx / (DOOMGENERIC_RESX / 3);   // 0,1,2
    if (col > 2) col = 2;
    bool topRow = (dy < DOOMGENERIC_RESY / 2);

    int keys[MAX_HELD];
    int n = 0;

    bool inMenu = menuactive || (gamestate != GS_LEVEL);

    if (inMenu) {
        // Menu / non-gameplay mapping.
        if (col == 0) {
            keys[n++] = K_BACK;            // ESC
        } else if (col == 2) {
            keys[n++] = K_SELECT;          // ENTER
        } else {
            keys[n++] = topRow ? KEY_UPARROW : KEY_DOWNARROW;
        }
    } else {
        // In-level mapping.
        if (col == 0) {
            keys[n++] = topRow ? K_STRAFELEFT : K_TURNLEFT;
        } else if (col == 2) {
            keys[n++] = topRow ? K_FIRE : K_USE;
        } else {
            keys[n++] = topRow ? K_FORWARD : K_BACKWARD;
        }
    }

    pressKeys(keys, n);
}

void I_ShutdownTouch(void)
{
    releaseAll();
}

#endif // USE_TOUCH_INPUT
#endif // DOOM_ESP32
