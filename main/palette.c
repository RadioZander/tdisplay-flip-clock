#include <string.h>
#include "palette.h"
#include "display.h"

// Rough order: light, bright, deep, dark, so similar colours sit together
static const palette_color_t colors[] = {
    {"white", "White", RGB565(255, 255, 255)},
    {"warm_white", "Warm white", RGB565(240, 232, 210)},
    {"cream", "Cream", RGB565(228, 220, 198)},
    {"yellow", "Yellow", RGB565(255, 230, 60)},
    {"amber", "Amber", RGB565(255, 176, 60)},
    {"orange", "Orange", RGB565(255, 120, 30)},
    {"red", "Red", RGB565(255, 60, 60)},
    {"pink", "Pink", RGB565(255, 120, 200)},
    {"lavender", "Lavender", RGB565(200, 170, 255)},
    {"purple", "Purple", RGB565(150, 70, 225)},
    {"blue", "Blue", RGB565(70, 130, 255)},
    {"cyan", "Cyan", RGB565(0, 220, 255)},
    {"green", "Green", RGB565(80, 255, 120)},
    {"forest", "Forest", RGB565(26, 68, 38)},
    {"navy", "Navy", RGB565(30, 42, 84)},
    {"plum", "Plum", RGB565(64, 32, 86)},
    {"maroon", "Maroon", RGB565(96, 22, 22)},
    {"wood", "Wood", RGB565(92, 60, 34)},
    {"charcoal", "Charcoal", RGB565(48, 48, 48)},
    {"black", "Black", RGB565(16, 16, 16)},
};

#define NUM_COLORS (int)(sizeof(colors) / sizeof(colors[0]))

int palette_count(void)
{
    return NUM_COLORS;
}

const palette_color_t *palette_get(int index)
{
    return &colors[((index % NUM_COLORS) + NUM_COLORS) % NUM_COLORS];
}

int palette_find(const char *id, int fallback)
{
    for (int i = 0; i < NUM_COLORS; i++) {
        if (strcmp(colors[i].id, id) == 0) {
            return i;
        }
    }
    return fallback;
}
