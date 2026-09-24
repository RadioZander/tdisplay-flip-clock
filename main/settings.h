#pragma once

#include <stdbool.h>

// User settings, changed from the on-device settings menu and kept in NVS
typedef struct {
    int digit_color; // palette indices
    int card_color;
    int date_color;
    int brightness;  // backlight percent: 10, 25, 50, 75 or 100
    bool hour12;     // 12-hour clock with AM/PM, otherwise 24-hour
    bool flipped;    // screen rotated 180 degrees
} clock_settings_t;

// Load saved settings, falling back to defaults for anything missing
void settings_load(clock_settings_t *s);
void settings_save(const clock_settings_t *s);

// Next (or previous) brightness level after `percent`, wrapping around
int settings_step_brightness(int percent, int direction);
