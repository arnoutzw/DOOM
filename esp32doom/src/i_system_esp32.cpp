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
//  ESP32 system-specific implementation
//
//-----------------------------------------------------------------------------

#ifdef DOOM_ESP32

#include <Arduino.h>
#include <stdarg.h>
#include <Wire.h>

#ifdef USE_TOUCH_INPUT
#include "i_touch.h"
#endif

extern "C" {
#include "doomdef.h"
#include "doomtype.h"
#include "d_event.h"
#include "d_ticcmd.h"
#include "i_system.h"
#include "m_misc.h"

// External function declarations
void D_PostEvent(event_t* ev);
}

// Button pins - defined in platformio.ini
#ifndef BUTTON_1
#define BUTTON_1 -1
#endif
#ifndef BUTTON_2
#define BUTTON_2 -1
#endif

// Zone memory size - ESP32 has limited RAM
#ifdef BOARD_HAS_PSRAM
#define ZONE_SIZE (4 * 1024 * 1024)  // 4MB with PSRAM
#else
#define ZONE_SIZE (256 * 1024)        // 256KB without PSRAM
#endif

// Base tic command
static ticcmd_t emptycmd;

// Start time
static unsigned long startTime = 0;

// Button states for debouncing
static bool lastButton1State = HIGH;
static bool lastButton2State = HIGH;

void I_Init(void)
{
    // Initialize serial for debugging
    Serial.begin(115200);
    Serial.println("DOOM for ESP32 initializing...");

    // Initialize buttons if available
#if BUTTON_1 >= 0
    pinMode(BUTTON_1, INPUT_PULLUP);
#endif
#if BUTTON_2 >= 0
    pinMode(BUTTON_2, INPUT_PULLUP);
#endif

#ifdef USE_TOUCH_INPUT
    // Initialize touch controller
    I_InitTouch();
    Serial.println("Touch input enabled");
#endif

    // Record start time
    startTime = millis();

    Serial.println("I_Init complete");
}

byte* I_ZoneBase(int* size)
{
    byte* zone = nullptr;

#ifdef BOARD_HAS_PSRAM
    // Allocate from PSRAM
    zone = (byte*)ps_malloc(ZONE_SIZE);
    if (zone) {
        Serial.printf("Zone memory allocated from PSRAM: %d bytes\n", ZONE_SIZE);
    }
#endif

    if (zone == nullptr) {
        // Fall back to regular malloc
        zone = (byte*)malloc(ZONE_SIZE);
        if (zone) {
            Serial.printf("Zone memory allocated from heap: %d bytes\n", ZONE_SIZE);
        }
    }

    if (zone == nullptr) {
        I_Error("Failed to allocate zone memory!");
    }

    *size = ZONE_SIZE;
    return zone;
}

int I_GetTime(void)
{
    // DOOM runs at 35 tics per second
    unsigned long elapsed = millis() - startTime;
    return (int)((elapsed * 35) / 1000);
}

void I_StartFrame(void)
{
    // Called at the start of each frame
    // Nothing needed for ESP32
}

void I_StartTic(void)
{
    event_t event;

#if BUTTON_1 >= 0
    // Read button 1 (typically used as fire/enter)
    bool button1State = digitalRead(BUTTON_1);
    if (button1State != lastButton1State) {
        event.type = button1State == LOW ? ev_keydown : ev_keyup;
        event.data1 = KEY_FIRE;  // Map to fire button
        event.data2 = 0;
        event.data3 = 0;
        D_PostEvent(&event);
        lastButton1State = button1State;
    }
#endif

#if BUTTON_2 >= 0
    // Read button 2 (typically used as use/back)
    bool button2State = digitalRead(BUTTON_2);
    if (button2State != lastButton2State) {
        event.type = button2State == LOW ? ev_keydown : ev_keyup;
        event.data1 = KEY_USE;  // Map to use button
        event.data2 = 0;
        event.data3 = 0;
        D_PostEvent(&event);
        lastButton2State = button2State;
    }
#endif

#ifdef USE_TOUCH_INPUT
    // Process touch input
    I_UpdateTouch();
#endif

    // Yield to other tasks
    yield();
}

ticcmd_t* I_BaseTiccmd(void)
{
    return &emptycmd;
}

void I_Quit(void)
{
    Serial.println("DOOM: Quit requested");

    // Clean shutdown
    extern void I_ShutdownGraphics(void);
    I_ShutdownGraphics();

    // Restart ESP32
    ESP.restart();
}

byte* I_AllocLow(int length)
{
    byte* mem = nullptr;

#ifdef BOARD_HAS_PSRAM
    mem = (byte*)ps_malloc(length);
#endif

    if (mem == nullptr) {
        mem = (byte*)malloc(length);
    }

    return mem;
}

void I_Tactile(int on, int off, int total)
{
    // Haptic feedback - not available on T-Display
    (void)on;
    (void)off;
    (void)total;
}

void I_Error(char* error, ...)
{
    char buffer[256];
    va_list args;

    va_start(args, error);
    vsnprintf(buffer, sizeof(buffer), error, args);
    va_end(args);

    Serial.println("DOOM Error:");
    Serial.println(buffer);

    // Flash the screen or LED to indicate error
#ifdef TFT_BL
    for (int i = 0; i < 10; i++) {
        digitalWrite(TFT_BL, LOW);
        delay(200);
        digitalWrite(TFT_BL, HIGH);
        delay(200);
    }
#endif

    // Halt
    while (1) {
        delay(1000);
    }
}

#endif // DOOM_ESP32
