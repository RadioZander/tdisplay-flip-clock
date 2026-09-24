// NTP clock for the LilyGO T-Display and T-Display-S3: connects to WiFi, syncs
// time over SNTP and shows it on the built-in ST7789 LCD.
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "buttons.h"
#include "palette.h"
#include "settings.h"
#include "display.h"
#include "flip_clock.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing main/secrets.h - copy main/secrets.example.h to main/secrets.h and add your WiFi details"
#endif

static const char *TAG = "ntp_clock";

#define FLIP_FRAMES   10  // animation frames per digit change
#define FLIP_FRAME_MS 15
#define MENU_TIMEOUT_MS 10000

static volatile bool s_wifi_connected;
static volatile bool s_time_synced;
static char s_ip[16] = "";

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected = false;
        s_ip[0] = '\0';
        ESP_LOGW(TAG, "WiFi disconnected, retrying...");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = data;
        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Got IP: %s", s_ip);
        s_wifi_connected = true;
    }
}

static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));

    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, WIFI_PASSWORD, sizeof(wifi_config.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Connecting to \"%s\"", WIFI_SSID);
}

static void time_sync_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time synchronised from %s", CONFIG_CLOCK_NTP_SERVER);
    s_time_synced = true;
}

static void sntp_init_client(void)
{
    // SNTP starts polling as soon as the network is up and re-syncs periodically
    // (CONFIG_LWIP_SNTP_UPDATE_DELAY, default 1 hour)
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_CLOCK_NTP_SERVER);
    config.sync_cb = time_sync_cb;
    ESP_ERROR_CHECK(esp_netif_sntp_init(&config));

    setenv("TZ", CONFIG_CLOCK_TIMEZONE, 1);
    tzset();
}

static clock_settings_t s_settings;

// On-device settings menu: long-press MENU to open or close (saves),
// short-press MENU for the next item, CHANGE to step the value forwards
// (long-press CHANGE steps backwards). Closes by itself after 10 s idle.
typedef enum {
    ITEM_DIGITS,
    ITEM_CARDS,
    ITEM_DATE,
    ITEM_BRIGHTNESS,
    ITEM_CLOCK,
    NUM_ITEMS,
} menu_item_t;

static const char *item_names[NUM_ITEMS] = {"Digits", "Cards", "Date", "Brightness", "Clock"};

static bool s_menu_open;
static menu_item_t s_menu_item;
static TickType_t s_menu_last_input;

static void apply_settings(void)
{
    flip_clock_style_t style = {
        .digit = palette_get(s_settings.digit_color)->rgb565,
        .card = palette_get(s_settings.card_color)->rgb565,
        .accent = palette_get(s_settings.date_color)->rgb565,
        .hour12 = s_settings.hour12,
    };
    flip_clock_set_style(&style);
    display_set_brightness(s_settings.brightness);
}

static void step_color(int *index, int direction)
{
    *index = (*index + direction + palette_count()) % palette_count();
}

static void menu_step_value(int direction)
{
    switch (s_menu_item) {
    case ITEM_DIGITS:
        step_color(&s_settings.digit_color, direction);
        break;
    case ITEM_CARDS:
        step_color(&s_settings.card_color, direction);
        break;
    case ITEM_DATE:
        step_color(&s_settings.date_color, direction);
        break;
    case ITEM_BRIGHTNESS:
        s_settings.brightness = settings_step_brightness(s_settings.brightness, direction);
        break;
    case ITEM_CLOCK:
        s_settings.hour12 = !s_settings.hour12;
        break;
    default:
        break;
    }
    apply_settings();
}

static void menu_value_text(char *buf, size_t len)
{
    switch (s_menu_item) {
    case ITEM_DIGITS:
        snprintf(buf, len, "%s", palette_get(s_settings.digit_color)->name);
        break;
    case ITEM_CARDS:
        snprintf(buf, len, "%s", palette_get(s_settings.card_color)->name);
        break;
    case ITEM_DATE:
        snprintf(buf, len, "%s", palette_get(s_settings.date_color)->name);
        break;
    case ITEM_BRIGHTNESS:
        snprintf(buf, len, "%d%%", s_settings.brightness);
        break;
    case ITEM_CLOCK:
        snprintf(buf, len, "%s", s_settings.hour12 ? "12 hour" : "24 hour");
        break;
    default:
        buf[0] = '\0';
        break;
    }
}

static void menu_close(void)
{
    s_menu_open = false;
    settings_save(&s_settings);
    ESP_LOGI(TAG, "Settings menu closed");
}

static void handle_button(const button_event_t *ev)
{
    s_menu_last_input = xTaskGetTickCount();

    if (!s_menu_open) {
        if (ev->button == BUTTON_MENU && ev->long_press) {
            s_menu_open = true;
            s_menu_item = ITEM_DIGITS;
            ESP_LOGI(TAG, "Settings menu opened");
        }
        return;
    }

    if (ev->button == BUTTON_MENU) {
        if (ev->long_press) {
            menu_close();
        } else {
            s_menu_item = (s_menu_item + 1) % NUM_ITEMS;
        }
    } else {
        menu_step_value(ev->long_press ? -1 : 1);
    }

    if (s_menu_open) {
        char value[24];
        menu_value_text(value, sizeof(value));
        ESP_LOGI(TAG, "%s: %s", item_names[s_menu_item], value);
    }
}

static bool menu_timed_out(void)
{
    return s_menu_open && xTaskGetTickCount() - s_menu_last_input > pdMS_TO_TICKS(MENU_TIMEOUT_MS);
}

static void draw_status_screen(int dots)
{
    char buf[32];
    display_clear(COLOR_BLACK);
    // Positions as a fraction of the screen height, to suit either board
    display_text_centered(DISPLAY_HEIGHT * 15 / 100, "NTP Clock", 3, COLOR_CYAN);

    const char *msg = s_wifi_connected ? "Syncing time" : "Connecting WiFi";
    snprintf(buf, sizeof(buf), "%s%.*s", msg, dots, "...");
    display_text(12, DISPLAY_HEIGHT * 52 / 100, buf, 2, COLOR_WHITE);

    if (s_wifi_connected) {
        snprintf(buf, sizeof(buf), "IP %s", s_ip);
    } else {
        snprintf(buf, sizeof(buf), "SSID %s", WIFI_SSID);
    }
    display_text_centered(DISPLAY_HEIGHT * 82 / 100, buf, 1, COLOR_GREY);
    display_flush();
}

static void draw_clock_screen(const struct tm *from, const struct tm *to, float t)
{
    char buf[32];
    display_clear(COLOR_BLACK);

    if (s_menu_open) {
        // Settings bar: "Item: value" in place of the status line
        char value[24];
        menu_value_text(value, sizeof(value));
        snprintf(buf, sizeof(buf), "%s: %s", item_names[s_menu_item], value);
        display_fill_rect(0, 0, DISPLAY_WIDTH, 26, COLOR_YELLOW);
        display_text_centered(5, buf, 2, COLOR_BLACK);
    } else {
        // Status line: WiFi indicator + timezone abbreviation (GMT/BST)
        strftime(buf, sizeof(buf), "%Z", to);
        display_text(4, 6, s_wifi_connected ? "WiFi OK" : "WiFi lost", 1,
                     s_wifi_connected ? COLOR_GREEN : COLOR_RED);
        display_text(DISPLAY_WIDTH - 4 - display_text_width(buf, 1), 6, buf, 1, COLOR_GREY);
    }

    flip_clock_draw(from, to, t);

    strftime(buf, sizeof(buf), "%a %d %b %Y", to);
    display_text_centered(flip_clock_bottom() + 12, buf, BOARD_DATE_SCALE, palette_get(s_settings.date_color)->rgb565);

    display_flush();
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    settings_load(&s_settings);
    display_init(s_settings.brightness);
    apply_settings();
    wifi_init();
    sntp_init_client();

    int dots = 0;
    while (!s_time_synced) {
        draw_status_screen(dots);
        dots = (dots + 1) % 4;
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    QueueHandle_t buttons = buttons_init();

    time_t last = time(NULL);
    struct tm shown;
    localtime_r(&last, &shown);
    draw_clock_screen(&shown, &shown, 1.0f);

    while (true) {
        time_t now = time(NULL);
        if (now != last) {
            last = now;
            struct tm next;
            localtime_r(&now, &next);
            for (int f = 1; f <= FLIP_FRAMES; f++) {
                draw_clock_screen(&shown, &next, (float)f / FLIP_FRAMES);
                vTaskDelay(pdMS_TO_TICKS(FLIP_FRAME_MS));
            }
            shown = next;
        }

        // Wait for the next tick, redrawing straight away if a button is pressed
        button_event_t ev;
        if (xQueueReceive(buttons, &ev, pdMS_TO_TICKS(20))) {
            handle_button(&ev);
            draw_clock_screen(&shown, &shown, 1.0f);
        } else if (menu_timed_out()) {
            menu_close();
            draw_clock_screen(&shown, &shown, 1.0f);
        }
    }
}
