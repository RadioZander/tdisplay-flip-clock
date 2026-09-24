#pragma once

#include <stdint.h>

// One colour list shared by the digits, cards and date. Each colour has a
// fixed id that is what gets saved, so adding or reordering colours never
// changes a saved choice.
typedef struct {
    const char *id;   // stored in NVS, keep stable (max 15 chars)
    const char *name; // shown on screen
    uint16_t rgb565;
} palette_color_t;

int palette_count(void);
const palette_color_t *palette_get(int index);

// Index of the colour with this id, or `fallback` if there is none
int palette_find(const char *id, int fallback);
