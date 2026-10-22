#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <freertos/idf_additions.h>
#include <nvs_flash.h>

#include "event_group_bits.h"
#include "mqtt/mqtt.h"
#include "sensor/ds18b20_sensor.h"
#include "wifi/wifi.h"

#define CHECK_EVENT_GROUP_BIT_TIME_INTERVAL 1

static const char *TAG = "MAIN";

void app_main(void) {
    ESP_LOGI(TAG, "Start");

    EventGroupHandle_t event_group = xEventGroupCreate();
    if (event_group == NULL) {
        ESP_LOGE(TAG, "No memory. Restart ...");
        esp_restart();
    }

    sensor_handle_t sensor_handle = {.event_group = event_group};
    sensor_init(&sensor_handle);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
            err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(err);
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_handle_t wifi_handle = {.event_group = event_group};
    ESP_ERROR_CHECK(wifi_init(&wifi_handle));

    mqtt_handle_t mqtt_handle = {.event_group = event_group};
    ESP_ERROR_CHECK(mqtt_init(&mqtt_handle));

    ESP_LOGI(TAG, "All ready!");

    float temperature = 0.f;

    for (;;) {
        EventBits_t bits = xEventGroupGetBits(event_group);
        if ((bits & MQTT_CONNECTED_BIT) == 0 ||
                (bits & SENSOR_READY_BIT) == 0) {
            vTaskDelay(
                    pdMS_TO_TICKS(1000 * CHECK_EVENT_GROUP_BIT_TIME_INTERVAL));
        }

        err = sensor_read_temperature(&sensor_handle, &temperature);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "read sensor error");
            vTaskDelay(pdMS_TO_TICKS(1000 * CONFIG_SENSOR_ERROR_SLEEP_TIME));
            continue;
        }

        err = mqtt_send_temperature(&mqtt_handle, temperature);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Temperature sent: %.2f", temperature);
        } else {
            ESP_LOGE(TAG, "MQTT publish error.");
            vTaskDelay(
                    pdMS_TO_TICKS(1000 * CONFIG_ESP_MQTT_LOST_WIFI_SLEEP_TIME));
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(1000 * CONFIG_SENSOR_MESSAGE_TIME_INTERVAL));
    }
}
