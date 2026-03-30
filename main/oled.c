// ESP-IDF official includes
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

#include "oled.h"

static const char *TAG = "oled";

#define OLED_I2C_PORT                0
#define OLED_PIXEL_CLOCK_HZ          (400 * 1000)
#define OLED_I2C_ADDR                0x3C
#define OLED_I2C_SDA_GPIO            18
#define OLED_I2C_SCL_GPIO            17
#define OLED_RESET_GPIO              -1
#define OLED_H_RES                   128
#define OLED_V_RES                   64

static bool s_oled_inited = false;
static lv_disp_t *s_disp = NULL;

static lv_obj_t *oled_get_active_screen(void)
{
#if LVGL_VERSION_MAJOR >= 9
	return lv_screen_active();
#else
	return lv_scr_act();
#endif
}

esp_err_t oled_init(void)
{
	if (s_oled_inited) {
		return ESP_OK;
	}

	esp_err_t ret;
	i2c_master_bus_handle_t i2c_bus = NULL;
	i2c_master_bus_config_t bus_config = {
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.glitch_ignore_cnt = 7,
		.i2c_port = OLED_I2C_PORT,
		.sda_io_num = OLED_I2C_SDA_GPIO,
		.scl_io_num = OLED_I2C_SCL_GPIO,
		.flags.enable_internal_pullup = true,
	};
	ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &i2c_bus), TAG, "i2c_new_master_bus failed");

	esp_lcd_panel_io_handle_t io_handle = NULL;
	esp_lcd_panel_io_i2c_config_t io_config = {
		.dev_addr = OLED_I2C_ADDR,
		.scl_speed_hz = OLED_PIXEL_CLOCK_HZ,
		.control_phase_bytes = 1,
		.lcd_cmd_bits = 8,
		.lcd_param_bits = 8,
		.dc_bit_offset = 6,
	};
	ret = esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &io_handle);
	if (ret != ESP_OK) {
		return ret;
	}

	esp_lcd_panel_handle_t panel_handle = NULL;
	esp_lcd_panel_dev_config_t panel_config = {
		.bits_per_pixel = 1,
		.reset_gpio_num = OLED_RESET_GPIO,
	};
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
	esp_lcd_panel_ssd1306_config_t ssd1306_cfg = {
		.height = OLED_V_RES,
	};
	panel_config.vendor_config = &ssd1306_cfg;
#endif

	ESP_RETURN_ON_ERROR(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle), TAG, "new_panel_ssd1306 failed");
	ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel_handle), TAG, "panel_reset failed");
	ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel_handle), TAG, "panel_init failed");
	//ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(panel_handle, false, true), TAG, "panel_mirror failed");
	ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel_handle, true), TAG, "panel_disp_on_off failed");

	const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
	ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "lvgl_port_init failed");

	const lvgl_port_display_cfg_t disp_cfg = {
		.io_handle = io_handle,
		.panel_handle = panel_handle,
		.buffer_size = OLED_H_RES * OLED_V_RES,
		.double_buffer = true,
		.hres = OLED_H_RES,
		.vres = OLED_V_RES,
		.monochrome = true,
#if LVGL_VERSION_MAJOR >= 9
		.color_format = LV_COLOR_FORMAT_RGB565,
#endif
		.rotation = {
			.swap_xy = false,
			.mirror_x = true,
			.mirror_y = true,
		},
		.flags = {
#if LVGL_VERSION_MAJOR >= 9
			.swap_bytes = false,
#endif
			.sw_rotate = false,
		}
	};
	s_disp = lvgl_port_add_disp(&disp_cfg);
	ESP_RETURN_ON_FALSE(s_disp != NULL, ESP_FAIL, TAG, "lvgl_port_add_disp failed");

	s_oled_inited = true;
	ESP_LOGI(TAG, "OLED initialized");
	return ESP_OK;
}

esp_err_t oled_show_text(const char *text)
{
	ESP_RETURN_ON_FALSE(s_oled_inited, ESP_ERR_INVALID_STATE, TAG, "oled not initialized");
	ESP_RETURN_ON_FALSE(text != NULL, ESP_ERR_INVALID_ARG, TAG, "text is null");

	if (!lvgl_port_lock(1000)) {
		return ESP_ERR_TIMEOUT;
	}

	lv_obj_t *screen = oled_get_active_screen();
	lv_obj_clean(screen);

	lv_obj_t *label = lv_label_create(screen);
	lv_label_set_text(label, text);
	lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);

	lvgl_port_unlock();
	return ESP_OK;
}

esp_err_t oled_show_status_screen(void)
{
	ESP_RETURN_ON_FALSE(s_oled_inited, ESP_ERR_INVALID_STATE, TAG, "oled not initialized");

	if (!lvgl_port_lock(1000)) {
		return ESP_ERR_TIMEOUT;
	}

	lv_obj_t *screen = oled_get_active_screen();
	lv_obj_clean(screen);

	lv_obj_t *line1 = lv_label_create(screen);
	lv_label_set_text(line1, "ESP-FOV-Driver");
	lv_obj_align(line1, LV_ALIGN_TOP_MID, 0, 0);

	lv_obj_t *line2 = lv_label_create(screen);
	lv_label_set_text(line2, "Stat-");
	lv_obj_align(line2, LV_ALIGN_TOP_LEFT, 0, 16);

	lv_obj_t *line3 = lv_label_create(screen);
	lv_label_set_text(line3, "BLDC");
	lv_obj_align(line3, LV_ALIGN_TOP_LEFT, 0, 32);

	lv_obj_t *line4 = lv_label_create(screen);
	lv_obj_set_width(line4, OLED_H_RES);
	lv_label_set_long_mode(line4, LV_LABEL_LONG_SCROLL_CIRCULAR);
	lv_label_set_text(line4, "AI for Design & Design for AI!");
	lv_obj_align(line4, LV_ALIGN_TOP_LEFT, 0, 48);

	lvgl_port_unlock();
	return ESP_OK;
}
