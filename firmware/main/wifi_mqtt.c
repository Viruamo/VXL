#include "wifi_mqtt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_client.h"

#define WIFI_SSID      "YourSSID"
#define WIFI_PASS      "YourPassword"
#define MQTT_BROKER    "mqtt://192.168.1.100"
#define DEVICE_ID      "nose01"

static const char *TAG = "wifi_mqtt";
static esp_mqtt_client_handle_t mqtt_client = NULL;

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "WiFi connected");
    }
}

void wifi_mqtt_init(void) {
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);

    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS
        }
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_cfg);
    esp_wifi_start();

    // Chờ kết nối WiFi tối đa 10 giây
    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connect failed");
        return;
    }

    // Cấu hình MQTT
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = MQTT_BROKER,
        .client_id = DEVICE_ID,
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(mqtt_client);
    ESP_LOGI(TAG, "MQTT started");
}

void mqtt_publish_sensor(float temperature, float humidity) {
    if (mqtt_client == NULL) return;
    char payload[128];
    // Tạo JSON đơn giản
    snprintf(payload, sizeof(payload),
             "{\"device_id\":\"%s\",\"ts\":%lld,\"temp\":%.2f,\"hum\":%.2f,\"gas\":[0.12,0.34,0.56]}",
             DEVICE_ID, (long long)time(NULL), temperature, humidity);
    esp_mqtt_client_publish(mqtt_client, "e-nose/nose01/data", payload, 0, 1, 0);
}
