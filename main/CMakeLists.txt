idf_component_register(SRCS "settings.c" "palette.c" "buttons.c" "flip_clock.c" "main.c" "display.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi esp_netif esp_event nvs_flash
                             esp_lcd esp_driver_spi esp_driver_gpio esp_driver_ledc)
