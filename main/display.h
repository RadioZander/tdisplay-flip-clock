#pragma once

#include <stdint.h>

// Landscape orientation, USB connector on the right
#define DISPLAY_WIDTH  240
#define DISPLAY_HEIGHT 135

// RGB565 colours
#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#define COLOR_BLACK  RGB565(0, 0, 0)
#define COLOR_WHITE  RGB565(255, 255, 255)
#define COLOR_GREY   RGB565(120, 120, 120)
#define COLOR_CYAN   RGB565(0, 220, 255)
#define COLOR_YELLOW RGB565(255, 210, 0)
#define COLOR_RED    RGB565(255, 60, 60)
#define COLOR_GREEN  RGB565(60, 220, 90)

// Set up the LCD and turn the backlight on at `brightness` percent
void display_init(int brightness);

// Backlight brightness, 0-100 %
void display_set_brightness(int percent);

// Drawing happens in an off-screen frame buffer; call display_flush() to show it
void display_clear(uint16_t color);
void display_fill_rect(int x, int y, int w, int h, uint16_t color);
void display_text(int x, int y, const char *text, int scale, uint16_t color);
void display_text_centered(int y, const char *text, int scale, uint16_t color);
int display_text_width(const char *text, int scale);
void display_flush(void);

// Direct access to the RGB565 frame buffer, DISPLAY_WIDTH * DISPLAY_HEIGHT pixels, row-major
uint16_t *display_framebuffer(void);
