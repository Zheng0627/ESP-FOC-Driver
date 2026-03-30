#pragma once

#include "esp_err.h"

esp_err_t oled_init(void);
esp_err_t oled_show_text(const char *text);
esp_err_t oled_show_status_screen(void);
