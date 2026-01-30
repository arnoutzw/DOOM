// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// DESCRIPTION:
//  Touch input handler for ESP32 touch displays
//
//-----------------------------------------------------------------------------

#ifndef __I_TOUCH__
#define __I_TOUCH__

#ifdef USE_TOUCH_INPUT

// Initialize touch controller
void I_InitTouch(void);

// Update touch input and generate events
void I_UpdateTouch(void);

// Shutdown touch controller
void I_ShutdownTouch(void);

#endif // USE_TOUCH_INPUT

#endif // __I_TOUCH__
