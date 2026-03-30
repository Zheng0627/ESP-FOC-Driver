// ESPRESSIF official ESP-IDF includes
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
// coustomer includes
#include "oled.h"

static const char *TAG = "main";

void app_main(void)
{
    if (oled_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "OLED init failed");
        return;
    }

    if (oled_show_status_screen() != ESP_OK)
    {
        ESP_LOGE(TAG, "OLED show status screen failed");
    }

}
