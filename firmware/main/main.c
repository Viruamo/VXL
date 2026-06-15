#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "sht3x_driver.h"
#include "filter_lib.h"
#include "wifi_mqtt.h"

static const char *TAG = "main";

// Bộ lọc tĩnh tránh phân mảnh heap
static median_filter_t temp_filter;
static median_filter_t humid_filter;

// Prototype phục hồi I2C
static void i2c_recovery(void);

void sensor_task(void *arg) {
    esp_task_wdt_add(NULL);
    float raw_t, raw_h;

    while (1) {
        esp_task_wdt_reset();

        esp_err_t ret = sht3x_read_raw(&raw_t, &raw_h);
        if (ret == ESP_OK) {
            float ft = filter_apply(&temp_filter, raw_t);
            float fh = filter_apply(&humid_filter, raw_h);
            // Gửi qua MQTT
            mqtt_publish_sensor(ft, fh);
            ESP_LOGI(TAG, "Temp: %.1f C, Hum: %.1f %%", ft, fh);
        } else {
            ESP_LOGE(TAG, "I2C error: %s", esp_err_to_name(ret));
            i2c_recovery();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void i2c_recovery(void) {
    // Đập 9 xung clock để giải phóng bus
    gpio_set_direction(21, GPIO_MODE_OUTPUT);
    gpio_set_direction(22, GPIO_MODE_OUTPUT);
    for (int i = 0; i < 9; i++) {
        gpio_set_level(22, 0);
        esp_rom_delay_us(5);
        gpio_set_level(22, 1);
        esp_rom_delay_us(5);
    }
    // Khởi tạo lại driver
    sht3x_deinit();
    vTaskDelay(pdMS_TO_TICKS(10));
    sht3x_init(21, 22);
}

void app_main(void) {
    ESP_LOGI(TAG, "E-nose starting");

    // Watchdog toàn cục 5 giây
    esp_task_wdt_config_t wdt_cfg = {
        .timeout_ms = 5000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic = true
    };
    esp_task_wdt_init(&wdt_cfg);

    filter_init(&temp_filter);
    filter_init(&humid_filter);

    if (sht3x_init(21, 22) != ESP_OK) {
        ESP_LOGE(TAG, "SHT3x init failed");
        return;
    }

    // Kết nối WiFi + MQTT
    wifi_mqtt_init();

    xTaskCreatePinnedToCore(sensor_task, "snsr", 4096, NULL, 5, NULL, 1);
}
