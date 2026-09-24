#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

// Vertical extent of the flip cards, for laying out text around them
#define FLIP_CLOCK_TOP    32
#define FLIP_CLOCK_BOTTOM (FLIP_CLOCK_TOP + 66)

// Draw HH MM SS as flip cards into the frame buffer.
// Digits that differ between `from` and `to` are drawn mid-flip at progress
// t (0.0 = showing `from`, 1.0 = showing `to`); unchanged digits are static.
void flip_clock_draw(const struct tm *from, const struct tm *to, float t);

typedef struct {
    uint16_t digit;  // RGB565 colours
    uint16_t card;   // top half; the bottom half is drawn a shade darker
    uint16_t accent; // AM/PM indicator
    bool hour12;     // 12-hour clock (blank leading zero, AM/PM), else 24-hour
} flip_clock_style_t;

void flip_clock_set_style(const flip_clock_style_t *style);
