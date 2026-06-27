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
//  Memory-map the IWAD directly out of a raw flash partition.
//
//  The ESP32-C6 has no PSRAM and only ~170KB of practically-usable heap, far
//  too little to cache WAD lumps in the zone. Instead the WAD is flashed to a
//  dedicated raw partition ("wad") and memory-mapped (XIP) so every lump can be
//  read in place, with zero RAM copies. w_wad.c returns pointers straight into
//  this mapping (see WAD_IN_FLASH), so the zone heap only ever has to hold the
//  parsed level geometry, not textures/sprites/flats/sounds.
//
//-----------------------------------------------------------------------------

#if defined(DOOM_ESP32) && defined(WAD_IN_FLASH)

#include <Arduino.h>
#include "esp_partition.h"

// Raw data partition that holds the IWAD image (see partitions_8mb.csv).
#define WAD_PARTITION_NAME    "wad"
#define WAD_PARTITION_SUBTYPE 0x40

// Cached so repeated calls (version detection + directory parse) map only once.
static const unsigned char* g_wad_base = NULL;
static int g_wad_size = 0;

extern "C" const unsigned char* I_MapWadFlash(int* out_size)
{
    if (g_wad_base) {
        if (out_size) *out_size = g_wad_size;
        return g_wad_base;
    }
    if (out_size) *out_size = 0;

    const esp_partition_t* part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        (esp_partition_subtype_t)WAD_PARTITION_SUBTYPE,
        WAD_PARTITION_NAME);

    if (!part) {
        Serial.println("WAD flash partition not found");
        return NULL;
    }

    const void* mapped = NULL;
    esp_partition_mmap_handle_t handle;
    esp_err_t err = esp_partition_mmap(
        part, 0, part->size, ESP_PARTITION_MMAP_DATA, &mapped, &handle);

    if (err != ESP_OK || !mapped) {
        Serial.printf("esp_partition_mmap failed: %d\n", (int)err);
        return NULL;
    }

    // Validate it actually contains a WAD (IWAD/PWAD magic) before committing.
    const unsigned char* base = (const unsigned char*)mapped;
    if (!(base[0] == 'I' || base[0] == 'P') ||
        base[1] != 'W' || base[2] != 'A' || base[3] != 'D') {
        Serial.println("WAD partition has no IWAD/PWAD signature");
        return NULL;  // leave it mapped; caller will fall back to SD/LittleFS
    }

    g_wad_base = base;
    g_wad_size = (int)part->size;
    if (out_size) *out_size = g_wad_size;
    Serial.printf("IWAD mmap'd from flash: %u bytes at %p\n",
                  (unsigned)part->size, mapped);
    return base;
}

#endif // DOOM_ESP32 && WAD_IN_FLASH
