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
//  Silent sound/music backend for the ESP32 ports.
//
//  Replaces the OSS (/dev/dsp) based linuxdoom i_sound.c, which cannot build on
//  the ESP32. All entry points are no-ops; the game runs without audio. The
//  function declarations come from i_sound.h (included with C linkage) so these
//  definitions link against the C engine.
//
//-----------------------------------------------------------------------------

#ifdef DOOM_ESP32

extern "C" {

#include "doomdef.h"
#include "doomstat.h"
#include "sounds.h"
#include "i_sound.h"

void I_InitSound(void) {}
void I_UpdateSound(void) {}
void I_SubmitSound(void) {}
void I_ShutdownSound(void) {}

void I_SetChannels(void) {}

int I_GetSfxLumpNum(sfxinfo_t* sfxinfo)
{
    (void)sfxinfo;
    return 0;
}

int I_StartSound(int id, int vol, int sep, int pitch, int priority)
{
    (void)id; (void)vol; (void)sep; (void)pitch; (void)priority;
    // Return a non-negative handle; I_SoundIsPlaying() always reports stopped,
    // so the channel is reclaimed on the next update.
    return 1;
}

void I_StopSound(int handle) { (void)handle; }

int I_SoundIsPlaying(int handle)
{
    (void)handle;
    return 0;
}

void I_UpdateSoundParams(int handle, int vol, int sep, int pitch)
{
    (void)handle; (void)vol; (void)sep; (void)pitch;
}

// Music.
void I_InitMusic(void) {}
void I_ShutdownMusic(void) {}
void I_SetMusicVolume(int volume) { (void)volume; }
void I_PauseSong(int handle) { (void)handle; }
void I_ResumeSong(int handle) { (void)handle; }

int I_RegisterSong(void* data)
{
    (void)data;
    return 1;
}

void I_PlaySong(int handle, int looping) { (void)handle; (void)looping; }
void I_StopSong(int handle) { (void)handle; }
void I_UnRegisterSong(int handle) { (void)handle; }

} // extern "C"

#endif // DOOM_ESP32
