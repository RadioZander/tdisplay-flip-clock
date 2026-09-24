#include "settings.h"
#include "palette.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "settings";
static const char *NVS_NAMESPACE = "settings";

static const int brightness_levels[] = {10, 25, 50, 75, 100};
#define NUM_LEVELS (int)(sizeof(brightness_levels) / sizeof(brightness_levels[0]))

static int load_color(nvs_handle_t nvs, const char *key, const char *default_id)
{
    char id[16];
    size_t len = sizeof(id);
    int fallback = palette_find(default_id, 0);
    if (nvs_get_str(nvs, key, id, &len) != ESP_OK) {
        return fallback;
    }
    return palette_find(id, fallback);
}

void settings_load(clock_settings_t *s)
{
    *s = (clock_settings_t) {
        .digit_color = palette_find("warm_white", 0),
        .card_color = palette_find("charcoal", 0),
        .date_color = palette_find("amber", 0),
        .brightness = 100,
        .hour12 = false,
    };

    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) {
        return; // nothing saved yet
    }
    s->digit_color = load_color(nvs, "digit", "warm_white");
    s->card_color = load_color(nvs, "card", "charcoal");
    s->date_color = load_color(nvs, "date", "amber");
    uint8_t v;
    if (nvs_get_u8(nvs, "brightness", &v) == ESP_OK) {
        s->brightness = v;
    }
    if (nvs_get_u8(nvs, "hour12", &v) == ESP_OK) {
        s->hour12 = v;
    }
    nvs_close(nvs);
}

void settings_save(const clock_settings_t *s)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return;
    }
    nvs_set_str(nvs, "digit", palette_get(s->digit_color)->id);
    nvs_set_str(nvs, "card", palette_get(s->card_color)->id);
    nvs_set_str(nvs, "date", palette_get(s->date_color)->id);
    nvs_set_u8(nvs, "brightness", s->brightness);
    nvs_set_u8(nvs, "hour12", s->hour12);
    err = nvs_commit(nvs);
    nvs_close(nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Saved");
    }
}

int settings_step_brightness(int percent, int direction)
{
    // Find the current level (or the nearest one above it) and step from there
    int i = 0;
    while (i < NUM_LEVELS - 1 && brightness_levels[i] < percent) {
        i++;
    }
    i = (i + direction + NUM_LEVELS) % NUM_LEVELS;
    return brightness_levels[i];
}
