// Retro split-flap clock cards.
//
// Each card is split at a hinge. When a digit changes, the top half of the
// old digit folds down towards the hinge (revealing the new top half behind
// it), then the bottom half of the new digit unfolds down over the old one.
#include <math.h>
#include <stdbool.h>
#include "sdkconfig.h"
#include "flip_clock.h"
#include "flip_font.h"
#include "display.h"

#define HINGE_COLOR       COLOR_BLACK
#define CORNER_RADIUS     4

#define BLANK             10 // digit value for an empty card (12-hour leading zero)

static uint16_t s_digit_color, s_card_top, s_card_bottom, s_accent_color;
static bool s_hour12;

typedef struct {
    int x, y;
    const flip_font_t *font;
} card_pos_t;

#define LARGE_Y FLIP_CLOCK_TOP
#if CONFIG_CLOCK_SECONDS_ON_HINGE
#define SMALL_Y (FLIP_CLOCK_TOP + (66 - 34) / 2) // hinges line up across all cards
#else
#define SMALL_Y (FLIP_CLOCK_BOTTOM - 34)         // bottom edges line up
#endif

// HH MM in large cards, SS in small cards
static const card_pos_t cards[6] = {
    {1, LARGE_Y, &flip_font_large},    {46, LARGE_Y, &flip_font_large},
    {98, LARGE_Y, &flip_font_large},   {143, LARGE_Y, &flip_font_large},
    {193, SMALL_Y, &flip_font_small},  {217, SMALL_Y, &flip_font_small},
};

// Linear blend of two RGB565 colours, alpha 0..255
static uint16_t blend(uint16_t bg, uint16_t fg, int alpha)
{
    int r = ((bg >> 11) * (255 - alpha) + (fg >> 11) * alpha) / 255;
    int g = (((bg >> 5) & 0x3F) * (255 - alpha) + ((fg >> 5) & 0x3F) * alpha) / 255;
    int b = ((bg & 0x1F) * (255 - alpha) + (fg & 0x1F) * alpha) / 255;
    return (r << 11) | (g << 5) | b;
}

void flip_clock_set_style(const flip_clock_style_t *style)
{
    s_digit_color = style->digit;
    s_card_top = style->card;
    s_card_bottom = blend(COLOR_BLACK, style->card, 215); // lower half in shadow
    s_accent_color = style->accent;
    s_hour12 = style->hour12;
}

// Pixels to leave blank at each end of card row `cy` to round the corners
static int corner_inset(int cy, int h)
{
    int dy = cy < CORNER_RADIUS ? CORNER_RADIUS - cy : cy >= h - CORNER_RADIUS ? cy - (h - CORNER_RADIUS - 1) : 0;
    int inset = 0;
    while (dy && inset < CORNER_RADIUS) {
        int dx = CORNER_RADIUS - inset;
        // Inside the circle if (dx-0.5)^2 + (dy-0.5)^2 <= r^2
        if ((2 * dx - 1) * (2 * dx - 1) + (2 * dy - 1) * (2 * dy - 1) <= 4 * CORNER_RADIUS * CORNER_RADIUS) {
            break;
        }
        inset++;
    }
    return inset;
}

// Copy row `src_cy` of a card showing `digit` to screen row `dst_y`,
// darkened by `light` (0..255) for the shading of a moving flap
static void draw_card_row(const card_pos_t *c, int dst_y, int digit, int src_cy, int light)
{
    const flip_font_t *f = c->font;
    if (dst_y < 0 || dst_y >= DISPLAY_HEIGHT) {
        return;
    }
    uint16_t base = src_cy < f->height / 2 ? s_card_top : s_card_bottom;
    const uint8_t *glyph = digit == BLANK ? NULL : f->digits[digit] + src_cy * f->width;
    uint16_t *dst = display_framebuffer() + dst_y * DISPLAY_WIDTH + c->x;
    int inset = corner_inset(src_cy, f->height);

    for (int cx = inset; cx < f->width - inset; cx++) {
        uint16_t px = glyph ? blend(base, s_digit_color, glyph[cx]) : base;
        dst[cx] = light < 255 ? blend(COLOR_BLACK, px, light) : px;
    }
}

// Draw a half-card flap squashed vertically by `scale` (0..1) against the hinge
static void draw_flap(const card_pos_t *c, int digit, bool top_half, float scale)
{
    int half = c->font->height / 2;
    int rows = (int)(half * scale + 0.5f);
    int light = 140 + (int)(115 * scale); // darker as it turns edge-on

    for (int r = 0; r < rows; r++) {
        int offset = (int)(r / scale);
        if (offset >= half) {
            offset = half - 1;
        }
        if (top_half) {
            draw_card_row(c, c->y + half - 1 - r, digit, half - 1 - offset, light);
        } else {
            draw_card_row(c, c->y + half + r, digit, half + offset, light);
        }
    }
}

static void draw_hinge(const card_pos_t *c)
{
    int half = c->font->height / 2;
    int thickness = c->font->height >= 60 ? 2 : 1;
    display_fill_rect(c->x, c->y + half - thickness / 2, c->font->width, thickness, HINGE_COLOR);
    // Little notches on each side where the flaps pivot
    display_fill_rect(c->x, c->y + half - 3, 2, 6, HINGE_COLOR);
    display_fill_rect(c->x + c->font->width - 2, c->y + half - 3, 2, 6, HINGE_COLOR);
}

static void draw_card(const card_pos_t *c, int from, int to, float t)
{
    int h = c->font->height;
    int half = h / 2;

    if (from == to || t >= 1.0f) {
        for (int cy = 0; cy < h; cy++) {
            draw_card_row(c, c->y + cy, to, cy, 255);
        }
    } else {
        // Behind the flap: new top half, old bottom half
        for (int cy = 0; cy < half; cy++) {
            draw_card_row(c, c->y + cy, to, cy, 255);
        }
        for (int cy = half; cy < h; cy++) {
            draw_card_row(c, c->y + cy, from, cy, 255);
        }
        // Projected height of a flap rotating about the hinge
        float scale = fabsf(cosf(t * (float)M_PI));
        if (t < 0.5f) {
            draw_flap(c, from, true, scale);
        } else {
            draw_flap(c, to, false, scale);
        }
    }
    draw_hinge(c);
}

static void time_to_digits(const struct tm *t, int d[6])
{
    int hour = t->tm_hour;
    if (s_hour12) {
        hour = hour % 12 == 0 ? 12 : hour % 12;
    }
    d[0] = s_hour12 && hour < 10 ? BLANK : hour / 10;
    d[1] = hour % 10;
    d[2] = t->tm_min / 10;
    d[3] = t->tm_min % 10;
    d[4] = t->tm_sec / 10;
    d[5] = t->tm_sec % 10;
}

// AM/PM in the space next to the hour/minute cards, above the seconds
static void draw_am_pm(const struct tm *t)
{
    const char *text = t->tm_hour < 12 ? "AM" : "PM";
    int left = cards[4].x;
    int width = cards[5].x + flip_font_small.width - left;
    int space = SMALL_Y - FLIP_CLOCK_TOP;
    int scale = space >= 24 ? 2 : 1;
    int x = left + (width - display_text_width(text, scale)) / 2;
    int y = FLIP_CLOCK_TOP + (space - 8 * scale) / 2;
    display_text(x, y, text, scale, s_accent_color);
}

void flip_clock_draw(const struct tm *from, const struct tm *to, float t)
{
    int a[6], b[6];
    time_to_digits(from, a);
    time_to_digits(to, b);
    for (int i = 0; i < 6; i++) {
        draw_card(&cards[i], a[i], b[i], t);
    }
    if (s_hour12) {
        draw_am_pm(to);
    }
}
