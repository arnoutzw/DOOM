// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// DESCRIPTION:
//  Touch input handler for ESP32 touch displays
//  Supports CST816S/CST816T capacitive touch controllers
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

void D_PostEvent(event_t* ev);
}

// Touch controller I2C address (CST816S/CST816T)
#define CST816_ADDR 0x15

// Touch pins - defined in platformio.ini
#ifndef TOUCH_SDA
#define TOUCH_SDA 6
#endif
#ifndef TOUCH_SCL
#define TOUCH_SCL 7
#endif
#ifndef TOUCH_INT
#define TOUCH_INT -1
#endif
#ifndef TOUCH_RST
#define TOUCH_RST -1
#endif

// Screen dimensions for touch mapping
#ifndef DOOMGENERIC_RESX
#define DOOMGENERIC_RESX 320
#endif
#ifndef DOOMGENERIC_RESY
#define DOOMGENERIC_RESY 240
#endif

// Touch zone definitions (screen divided into regions)
// Layout:
// +-------+-------+-------+
// | STRFE | FORWD | FIRE  |
// | LEFT  |       |       |
// +-------+-------+-------+
// | TURN  | BACK  | USE   |
// | LEFT  |       |       |
// +-------+-------+-------+
//
// Left third: movement (strafe/turn)
// Middle third: forward/backward
// Right third: fire/use

#define ZONE_LEFT_X   (DOOMGENERIC_RESX / 3)
#define ZONE_RIGHT_X  (DOOMGENERIC_RESX * 2 / 3)
#define ZONE_MID_Y    (DOOMGENERIC_RESY / 2)

// Touch state tracking
static bool touchActive = false;
static int lastTouchX = -1;
static int lastTouchY = -1;

// Key states for detecting changes
static bool keyForward = false;
static bool keyBackward = false;
static bool keyLeft = false;
static bool keyRight = false;
static bool keyFire = false;
static bool keyUse = false;
static bool keyStrafeLeft = false;
static bool keyStrafeRight = false;

// Read touch data from CST816
static bool readTouch(int* x, int* y, bool* pressed)
{
    Wire.beginTransmission(CST816_ADDR);
    Wire.write(0x01);  // Register for touch data
    if (Wire.endTransmission() != 0) {
        return false;
    }

    Wire.requestFrom(CST816_ADDR, 6);
    if (Wire.available() < 6) {
        return false;
    }

    uint8_t gesture = Wire.read();
    uint8_t touchPoints = Wire.read();
    uint8_t xHigh = Wire.read();
    uint8_t xLow = Wire.read();
    uint8_t yHigh = Wire.read();
    uint8_t yLow = Wire.read();

    *pressed = (touchPoints > 0);
    *x = ((xHigh & 0x0F) << 8) | xLow;
    *y = ((yHigh & 0x0F) << 8) | yLow;

    return true;
}

static void postKeyEvent(int key, bool pressed)
{
    event_t event;
    event.type = pressed ? ev_keydown : ev_keyup;
    event.data1 = key;
    event.data2 = 0;
    event.data3 = 0;
    D_PostEvent(&event);
}

void I_InitTouch(void)
{
    // Initialize I2C for touch controller
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    Wire.setClock(400000);  // 400kHz I2C

    // Reset touch controller if reset pin available
#if TOUCH_RST >= 0
    pinMode(TOUCH_RST, OUTPUT);
    digitalWrite(TOUCH_RST, LOW);
    delay(10);
    digitalWrite(TOUCH_RST, HIGH);
    delay(50);
#endif

    // Setup interrupt pin if available
#if TOUCH_INT >= 0
    pinMode(TOUCH_INT, INPUT);
#endif

    Serial.println("Touch controller initialized");
}

void I_UpdateTouch(void)
{
    int x, y;
    bool pressed;

    if (!readTouch(&x, &y, &pressed)) {
        return;
    }

    // Handle touch release - release all keys
    if (!pressed && touchActive) {
        touchActive = false;

        if (keyForward) { postKeyEvent(KEY_UPARROW, false); keyForward = false; }
        if (keyBackward) { postKeyEvent(KEY_DOWNARROW, false); keyBackward = false; }
        if (keyLeft) { postKeyEvent(KEY_LEFTARROW, false); keyLeft = false; }
        if (keyRight) { postKeyEvent(KEY_RIGHTARROW, false); keyRight = false; }
        if (keyFire) { postKeyEvent(KEY_FIRE, false); keyFire = false; }
        if (keyUse) { postKeyEvent(KEY_USE, false); keyUse = false; }
        if (keyStrafeLeft) { postKeyEvent(',', false); keyStrafeLeft = false; }
        if (keyStrafeRight) { postKeyEvent('.', false); keyStrafeRight = false; }

        return;
    }

    if (!pressed) {
        return;
    }

    touchActive = true;
    lastTouchX = x;
    lastTouchY = y;

    // Determine which zone was touched
    bool newForward = false;
    bool newBackward = false;
    bool newLeft = false;
    bool newRight = false;
    bool newFire = false;
    bool newUse = false;
    bool newStrafeLeft = false;
    bool newStrafeRight = false;

    if (x < ZONE_LEFT_X) {
        // Left zone: strafe left (top) / turn left (bottom)
        if (y < ZONE_MID_Y) {
            newStrafeLeft = true;
        } else {
            newLeft = true;
        }
    } else if (x > ZONE_RIGHT_X) {
        // Right zone: fire (top) / use (bottom)
        if (y < ZONE_MID_Y) {
            newFire = true;
        } else {
            newUse = true;
        }
    } else {
        // Middle zone: forward (top) / backward (bottom)
        if (y < ZONE_MID_Y) {
            newForward = true;
        } else {
            newBackward = true;
        }
    }

    // Post events for state changes
    if (newForward != keyForward) {
        postKeyEvent(KEY_UPARROW, newForward);
        keyForward = newForward;
    }
    if (newBackward != keyBackward) {
        postKeyEvent(KEY_DOWNARROW, newBackward);
        keyBackward = newBackward;
    }
    if (newLeft != keyLeft) {
        postKeyEvent(KEY_LEFTARROW, newLeft);
        keyLeft = newLeft;
    }
    if (newRight != keyRight) {
        postKeyEvent(KEY_RIGHTARROW, newRight);
        keyRight = newRight;
    }
    if (newFire != keyFire) {
        postKeyEvent(KEY_FIRE, newFire);
        keyFire = newFire;
    }
    if (newUse != keyUse) {
        postKeyEvent(KEY_USE, newUse);
        keyUse = newUse;
    }
    if (newStrafeLeft != keyStrafeLeft) {
        postKeyEvent(',', newStrafeLeft);
        keyStrafeLeft = newStrafeLeft;
    }
    if (newStrafeRight != keyStrafeRight) {
        postKeyEvent('.', newStrafeRight);
        keyStrafeRight = newStrafeRight;
    }
}

void I_ShutdownTouch(void)
{
    // Nothing to clean up
}

#endif // USE_TOUCH_INPUT
#endif // DOOM_ESP32
