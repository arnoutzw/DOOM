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
//  Single-player network stub for the ESP32 ports.
//
//  Replaces the BSD-socket linuxdoom i_net.c. Only the local single-player
//  configuration is supported: I_InitNetwork allocates and initialises doomcom
//  exactly as the original does for a non "-net" game, and I_NetCmd is a no-op.
//
//-----------------------------------------------------------------------------

#ifdef DOOM_ESP32

#include <stdlib.h>
#include <string.h>

extern "C" {

#include "doomdef.h"
#include "doomstat.h"
#include "d_net.h"
#include "i_net.h"

// Defined in d_net.c.
extern doomcom_t* doomcom;

void I_InitNetwork(void)
{
    doomcom = (doomcom_t*)malloc(sizeof(*doomcom));
    memset(doomcom, 0, sizeof(*doomcom));

    doomcom->ticdup = 1;
    doomcom->extratics = 0;

    // Single player game.
    netgame = false;
    doomcom->id = DOOMCOM_ID;
    doomcom->numplayers = doomcom->numnodes = 1;
    doomcom->deathmatch = false;
    doomcom->consoleplayer = 0;
}

void I_NetCmd(void)
{
    // No networking in the single-player ESP32 build.
}

} // extern "C"

#endif // DOOM_ESP32
